#include <somni/core/SimulationKernel.hpp>
#include <spdlog/spdlog.h>
#include <fstream>
#include <stdexcept>
#include <thread>

namespace somni {

SimulationKernel::SimulationKernel(KernelConfig cfg) : cfg_(std::move(cfg)) {
    spdlog::set_level(cfg_.log_verbose ? spdlog::level::debug : spdlog::level::info);
    spdlog::info("SOMNI kernel created");
}

SimulationKernel::~SimulationKernel() {
    if (tick_engine_ && tick_engine_->is_running()) stop();
}

void SimulationKernel::bootstrap(const WorldBootstrap& spec) {
    if (bootstrapped_) throw std::runtime_error("Kernel already bootstrapped");

    spdlog::info("SOMNI bootstrap: world='{}' seed={} size={}x{}",
                 spec.world_config.name, spec.world_config.seed,
                 spec.world_config.width, spec.world_config.height);

    // STEP 3a: Generate world
    world_ = std::make_unique<WorldState>(spec.world_config);
    TerrainGenerator gen(spec.world_config.seed);
    gen.generate(*world_);
    RegionClassifier::classify(*world_);

    // STEP 3b: Spatial query layer
    map_ = std::make_unique<WorldMap>(*world_);

    // STEP 3c: Faction registry
    faction_registry_ = std::make_unique<FactionRegistry>();
    for (const auto& fs : spec.factions) {
        FactionData fd;
        fd.ideology    = fs.ideology;
        fd.government  = fs.government;
        fd.treasury    = fs.initial_treasury;
        fd.population  = fs.initial_population;
        fd.stockpile   = fs.initial_resources;
        fd.name_hash   = std::hash<std::string>{}(fs.name);
        faction_registry_->create_faction(fd);
    }

    // Set up initial diplomacy (all factions neutral by default)
    auto ids = faction_registry_->all_ids();
    for (size_t i = 0; i < ids.size(); ++i) {
        for (size_t j = i + 1; j < ids.size(); ++j) {
            faction_registry_->diplomacy().set_relation(ids[i], ids[j], 0.0f);
        }
    }

    // STEP 4a: Society systems
    economy_system_ = std::make_unique<EconomySystem>(*world_, *faction_registry_, bus_);
    conflict_system_ = std::make_unique<ConflictSystem>(
        *world_, *faction_registry_, bus_, spec.world_config.seed);
    diplomacy_system_ = std::make_unique<DiplomacySystem>(
        *faction_registry_, *conflict_system_, bus_, spec.world_config.seed);

    // STEP 4b: NPC system
    npc_system_ = std::make_unique<NPCSystem>(
        *world_, *map_, *faction_registry_, bus_, spec.world_config.seed);

    npc_system_->initialize();  // builds BT factory in-place (avoids move/copy)

    // STEP 4c: Spawn initial population
    spawn_initial_population(spec);

    // Wire tick engine
    tick_engine_ = std::make_unique<TickEngine>(cfg_.tick);
    wire_systems();
    schedule_scripted_events(spec);

    // Snapshot scheduling
    if (cfg_.snapshot_interval_ticks > 0) {
        tick_engine_->schedule_snapshot(cfg_.snapshot_interval_ticks,
            [this](uint64_t tick) {
                std::string path = cfg_.snapshot_dir + "snap_" + std::to_string(tick) + ".somni";
                save_snapshot(path);
            });
    }

    bootstrapped_ = true;
    spdlog::info("SOMNI bootstrap complete: {} regions, {} factions",
                 world_->config().total_regions(), faction_registry_->count());
}

void SimulationKernel::wire_systems() {
    // System priority defines execution order per tick
    tick_engine_->register_system({
        "world_clock", 10,
        [this](uint64_t tick) {
            world_->clock().advance(1);
            world_->tick_environment();
            world_->tick_resources();
        }
    });

    tick_engine_->register_system({
        "npc_system", 20,
        [this](uint64_t tick) {
            npc_system_->tick(registry_, tick);
        }
    });

    tick_engine_->register_system({
        "economy", 30,
        [this](uint64_t tick) {
            economy_system_->tick(tick);
        }
    });

    tick_engine_->register_system({
        "conflict", 40,
        [this](uint64_t tick) {
            conflict_system_->tick(tick);
        }
    });

    tick_engine_->register_system({
        "diplomacy", 50,
        [this](uint64_t tick) {
            diplomacy_system_->tick(tick);
        }
    });

    tick_engine_->register_system({
        "faction_tick", 60,
        [this](uint64_t tick) {
            faction_registry_->tick(tick);
        }
    });
}

void SimulationKernel::spawn_initial_population(const WorldBootstrap& spec) {
    auto faction_ids = faction_registry_->all_ids();
    for (size_t fi = 0; fi < spec.factions.size() && fi < faction_ids.size(); ++fi) {
        const auto& fs    = spec.factions[fi];
        FactionID   fid   = faction_ids[fi];

        // Assign roles: 40% gatherers, 20% farmers, 20% soldiers, 10% traders, 10% others
        static const SocialRole role_distribution[] = {
            SocialRole::GATHERER, SocialRole::GATHERER, SocialRole::GATHERER,
            SocialRole::GATHERER,
            SocialRole::FARMER,   SocialRole::FARMER,
            SocialRole::SOLDIER,  SocialRole::SOLDIER,
            SocialRole::TRADER,
            SocialRole::CRAFTSMAN,
        };
        constexpr uint32_t DIST_SIZE = sizeof(role_distribution) / sizeof(role_distribution[0]);

        for (uint32_t n = 0; n < spec.initial_npc_per_faction; ++n) {
            SocialRole role = role_distribution[n % DIST_SIZE];
            // Scatter NPCs around faction spawn point
            int32_t dx = static_cast<int32_t>((n % 10) - 5);
            int32_t dy = static_cast<int32_t>((n / 10) - (spec.initial_npc_per_faction / 20));
            npc_system_->spawn(registry_, fs.spawn_x + dx, fs.spawn_y + dy, fid, role, 0);
        }

        spdlog::info("Spawned {} NPCs for faction {}", spec.initial_npc_per_faction, fid);
    }
}

void SimulationKernel::schedule_scripted_events(const WorldBootstrap& spec) {
    for (const auto& ev : spec.scripted_events) {
        tick_engine_->schedule_snapshot(ev.trigger_tick,
            [this, ev](uint64_t tick) {
                spdlog::info("Scripted event '{}' triggered at tick {} region {}",
                             ev.type, tick, ev.region_id);
                if (ev.type == "disaster") {
                    auto& r = world_->region(ev.region_id);
                    for (uint8_t res = 0; res < 16; ++res)
                        r.resource_amount[res] *= (1.0f - ev.magnitude);
                } else if (ev.type == "plague") {
                    // plague lowers stability of all factions in region
                    auto& r = world_->region(ev.region_id);
                    auto* fd = faction_registry_->get(r.controlling_faction);
                    if (fd) fd->stability -= ev.magnitude * 0.3f;
                }
            });
    }
}

void SimulationKernel::start() {
    if (!bootstrapped_) throw std::runtime_error("Call bootstrap() before start()");
    spdlog::info("SOMNI simulation starting (headless={})", cfg_.tick.headless);
    std::thread([this]() { tick_engine_->run(); }).detach();
}

void SimulationKernel::stop()   { if (tick_engine_) tick_engine_->stop(); }
void SimulationKernel::pause()  { if (tick_engine_) tick_engine_->pause(); }
void SimulationKernel::resume() { if (tick_engine_) tick_engine_->resume(); }

uint64_t SimulationKernel::current_tick() const {
    return tick_engine_ ? tick_engine_->current_tick() : 0;
}
bool SimulationKernel::is_running() const {
    return tick_engine_ && tick_engine_->is_running();
}

void SimulationKernel::player_command(const PlayerCommandEvent& cmd) {
    bus_.emit(cmd);
}

void SimulationKernel::save_snapshot(const std::string& path) const {
    world_->save(path);
    spdlog::info("Snapshot saved: {}", path);
}

}  // namespace somni

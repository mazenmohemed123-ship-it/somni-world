#include <somni/core/WorldState.hpp>
#include <fstream>
#include <stdexcept>

namespace somni {

// ---------------------------------------------------------------------------
// WorldClock
// ---------------------------------------------------------------------------

void WorldClock::advance(uint64_t n_ticks) {
    tick += n_ticks;
    uint64_t t = tick;
    hour   = static_cast<uint8_t>((t / TICKS_PER_HOUR)  % 24);
    day    = static_cast<uint8_t>((t / TICKS_PER_DAY)   % 30 + 1);
    month  = static_cast<uint8_t>((t / TICKS_PER_MONTH) % 12 + 1);
    season = static_cast<uint8_t>((t / TICKS_PER_SEASON) % 4);
    year   = static_cast<uint32_t>(t / TICKS_PER_YEAR + 1);
}

nlohmann::json WorldClock::to_json() const {
    return {
        {"tick", tick}, {"year", year}, {"season", season},
        {"month", month}, {"day", day}, {"hour", hour}
    };
}

// ---------------------------------------------------------------------------
// WorldConfig
// ---------------------------------------------------------------------------

nlohmann::json WorldConfig::to_json() const {
    return {
        {"seed", seed}, {"name", name},
        {"width", width}, {"height", height},
        {"region_size", region_size},
        {"ticks_per_second", ticks_per_second},
        {"time_scale", time_scale}
    };
}

WorldConfig WorldConfig::from_json(const nlohmann::json& j) {
    WorldConfig c;
    c.seed             = j.value("seed", uint64_t{42});
    c.name             = j.value("name", std::string{"unnamed"});
    c.width            = j.value("width", 512);
    c.height           = j.value("height", 512);
    c.region_size      = j.value("region_size", 16u);
    c.ticks_per_second = j.value("ticks_per_second", 20u);
    c.time_scale       = j.value("time_scale", 1.0f);
    return c;
}

// ---------------------------------------------------------------------------
// WorldState
// ---------------------------------------------------------------------------

WorldState::WorldState(WorldConfig config) : config_(std::move(config)) {
    terrain_.resize(config_.total_cells());
    regions_.resize(config_.total_regions());

    int32_t rw = config_.region_grid_w();
    for (uint32_t rid = 0; rid < config_.total_regions(); ++rid) {
        auto& r   = regions_[rid];
        r.id      = rid;
        r.grid_x  = static_cast<int32_t>(rid % rw);
        r.grid_y  = static_cast<int32_t>(rid / rw);
    }
}

void WorldState::tick_resources() {
    for (auto& r : regions_) {
        for (uint8_t res = 0; res < 16; ++res) {
            if (r.resource_cap[res] > 0.0f) {
                float regen = r.resource_regen[res] * r.env.food_yield_mult;
                r.resource_amount[res] = std::min(
                    r.resource_amount[res] + regen, r.resource_cap[res]);
            }
        }
    }
}

void WorldState::tick_environment() {
    float season_food_mult[4] = {1.1f, 1.3f, 0.9f, 0.5f};  // spring/summer/autumn/winter
    float season_movement[4]  = {1.0f, 1.0f, 1.1f, 1.5f};

    float food_mult  = season_food_mult[clock_.season];
    float move_cost  = season_movement[clock_.season];

    for (auto& r : regions_) {
        r.env.food_yield_mult      = food_mult;
        r.env.movement_cost        = move_cost;
        r.env.water_availability   = (clock_.season == 3) ? 0.7f : 1.0f;
        r.env.sickness_risk        = (clock_.season == 3) ? 0.002f : 0.0005f;
    }
}

nlohmann::json WorldState::to_json() const {
    nlohmann::json j;
    j["config"] = config_.to_json();
    j["clock"]  = clock_.to_json();

    auto& rj = j["regions"];
    for (const auto& r : regions_) {
        nlohmann::json rjj;
        rjj["id"]                   = r.id;
        rjj["biome"]                = static_cast<uint8_t>(r.dominant_biome);
        rjj["controlling_faction"]  = r.controlling_faction;
        rjj["npc_count"]            = r.npc_count;
        std::vector<float> amounts(r.resource_amount.begin(), r.resource_amount.end());
        rjj["resources"] = amounts;
        rj.push_back(rjj);
    }
    return j;
}

void WorldState::save(const std::string& path) const {
    std::ofstream f(path);
    if (!f) throw std::runtime_error("Cannot open " + path);
    f << to_json().dump(2);
}

WorldState WorldState::load(const std::string& path) {
    std::ifstream f(path);
    if (!f) throw std::runtime_error("Cannot open " + path);
    nlohmann::json j;
    f >> j;
    return from_json(j);
}

WorldState WorldState::from_json(const nlohmann::json& j) {
    WorldState ws(WorldConfig::from_json(j["config"]));
    // clock
    const auto& ck = j["clock"];
    ws.clock_.tick   = ck["tick"];
    ws.clock_.year   = ck["year"];
    ws.clock_.season = ck["season"];
    ws.clock_.month  = ck["month"];
    ws.clock_.day    = ck["day"];
    ws.clock_.hour   = ck["hour"];

    if (j.contains("regions")) {
        for (const auto& rj : j["regions"]) {
            uint32_t id = rj["id"];
            auto& r = ws.regions_[id];
            r.dominant_biome     = static_cast<Biome>(rj["biome"].get<uint8_t>());
            r.controlling_faction = rj["controlling_faction"];
            r.npc_count          = rj["npc_count"];
            auto res = rj["resources"].get<std::vector<float>>();
            for (size_t i = 0; i < std::min(res.size(), r.resource_amount.size()); ++i)
                r.resource_amount[i] = res[i];
        }
    }
    return ws;
}

}  // namespace somni

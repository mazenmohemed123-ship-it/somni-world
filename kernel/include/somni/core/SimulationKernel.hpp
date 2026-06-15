#pragma once
#include <somni/core/EventBus.hpp>
#include <somni/core/WorldState.hpp>
#include <somni/core/TickEngine.hpp>
#include <somni/world/Geography.hpp>
#include <somni/world/WorldMap.hpp>
#include <somni/agents/NPC.hpp>
#include <somni/societies/Faction.hpp>
#include <somni/societies/Economy.hpp>
#include <somni/societies/Conflict.hpp>
#include <entt/entt.hpp>
#include <memory>
#include <string>
#include <functional>

namespace somni {

// ---------------------------------------------------------------------------
// Bootstrap descriptor — produced by Python orchestrator, consumed by kernel
// Fully deterministic from this struct + seed
// ---------------------------------------------------------------------------
struct WorldBootstrap {
    WorldConfig world_config;

    struct FactionSpec {
        std::string      name;
        FactionIdeology  ideology;
        GovernmentType   government;
        int32_t          spawn_x, spawn_y;     // starting location
        uint32_t         initial_population;
        float            initial_treasury;
        std::array<float, 16> initial_resources{};
    };
    std::vector<FactionSpec> factions;

    struct EventSpec {
        uint64_t    trigger_tick;
        std::string type;   // "disaster", "plague", "trade_boom", "invasion"
        uint32_t    region_id;
        float       magnitude;
    };
    std::vector<EventSpec> scripted_events;

    // Technical options
    uint32_t initial_npc_per_faction{50};
    bool     enable_fantasy_resources{false};
};

// ---------------------------------------------------------------------------
// KernelConfig — runtime configuration
// ---------------------------------------------------------------------------
struct KernelConfig {
    TickConfig  tick;
    std::string snapshot_dir{"worlds/"};
    uint32_t    snapshot_interval_ticks{1000};
    bool        log_verbose{false};
};

// ---------------------------------------------------------------------------
// SimulationKernel — the root object
//
// Owns all subsystems and wires them together.
// Creation sequence enforced:
//   1. construct kernel
//   2. call bootstrap(WorldBootstrap)  → world generated
//   3. call start()                    → simulation loop begins
// ---------------------------------------------------------------------------
class SimulationKernel {
public:
    explicit SimulationKernel(KernelConfig cfg = {});
    ~SimulationKernel();

    // STEP 3: World generation + agent initialization
    void bootstrap(const WorldBootstrap& spec);

    // STEP 5: Start simulation loop (non-blocking, runs on background thread)
    void start();
    void stop();
    void pause();
    void resume();

    // STEP 6: Player interaction
    void player_command(const PlayerCommandEvent& cmd);

    // State queries (thread-safe snapshot reads)
    uint64_t           current_tick()  const;
    bool               is_running()    const;
    const WorldState&  world()         const { return *world_; }
    entt::registry&    registry()            { return registry_; }
    FactionRegistry&   factions()            { return *faction_registry_; }
    EventBus&          event_bus()           { return bus_; }

    // Serialization
    void save_snapshot(const std::string& path) const;
    static SimulationKernel load_snapshot(const std::string& path, KernelConfig cfg = {});

    // Observe simulation events (for Python API layer)
    template <typename E>
    EventBus::HandlerID on(std::function<void(const E&)> handler) {
        return bus_.subscribe<E>(std::move(handler));
    }

private:
    void wire_systems();
    void spawn_initial_population(const WorldBootstrap& spec);
    void schedule_scripted_events(const WorldBootstrap& spec);

    KernelConfig         cfg_;
    EventBus             bus_;

    std::unique_ptr<WorldState>       world_;
    std::unique_ptr<WorldMap>         map_;
    std::unique_ptr<FactionRegistry>  faction_registry_;
    std::unique_ptr<NPCSystem>        npc_system_;
    std::unique_ptr<EconomySystem>    economy_system_;
    std::unique_ptr<ConflictSystem>   conflict_system_;
    std::unique_ptr<DiplomacySystem>  diplomacy_system_;
    std::unique_ptr<TickEngine>       tick_engine_;

    entt::registry registry_;
    bool           bootstrapped_{false};
};

}  // namespace somni

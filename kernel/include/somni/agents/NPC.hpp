#pragma once
#include <somni/agents/Needs.hpp>
#include <somni/agents/Memory.hpp>
#include <somni/agents/StateMachine.hpp>
#include <somni/agents/BehaviorTree.hpp>
#include <somni/world/Resources.hpp>
#include <somni/core/EventBus.hpp>   // NPCDiedEvent::Cause used in kill()
#include <entt/entt.hpp>
#include <cstdint>

namespace somni {

class WorldState;
class WorldMap;
class FactionRegistry;

// ---------------------------------------------------------------------------
// Social role — determines which BT subtree is used for "work"
// ---------------------------------------------------------------------------
enum class SocialRole : uint8_t {
    GATHERER  = 0,   // harvests raw resources
    FARMER    = 1,   // cultivates food
    HUNTER    = 2,   // hunts animals
    FISHER    = 3,   // fishes
    MINER     = 4,   // extracts ore/stone
    SOLDIER   = 5,   // defends territory, fights
    TRADER    = 6,   // conducts inter-region trade
    CRAFTSMAN = 7,   // converts raw → manufactured resources
    LEADER    = 8,   // influences faction decisions
    HEALER    = 9,   // restores needs of nearby NPCs
    BUILDER   = 10,  // constructs structures (future)
    NONE      = 0xFF
};

// ---------------------------------------------------------------------------
// ECS components — all plain data, no virtual dispatch in hot path
// ---------------------------------------------------------------------------

struct PositionComponent {
    int32_t  x{0};
    int32_t  y{0};
    uint32_t region_id{0};
};

struct SocialRoleComponent {
    SocialRole role{SocialRole::GATHERER};
    uint32_t   employer_faction{0xFFFFFFFF};  // who this NPC works for
};

struct FactionComponent {
    uint32_t faction_id{0xFFFFFFFF};
    float    loyalty{0.8f};  // [0,1] — low loyalty → risk of defection
};

struct CombatComponent {
    float    attack{5.0f};
    float    defense{3.0f};
    float    health{1.0f};   // [0,1]
    uint32_t kills{0};
    bool     is_alive{true};
};

struct AgeComponent {
    uint32_t birth_tick{0};
    uint32_t lifespan_ticks{0};  // die of old age at this tick count
};

// ---------------------------------------------------------------------------
// NPC archetype — defines initial stats for a role
// ---------------------------------------------------------------------------
struct NPCArchetype {
    SocialRole role;
    NeedsDecayComponent decay;
    CombatComponent     combat;
    uint32_t            lifespan_ticks;  // typical lifespan in ticks

    static NPCArchetype for_role(SocialRole r);
};

// ---------------------------------------------------------------------------
// NPC system — orchestrates all NPC components each tick
// Runs in ECS view iteration order (cache-friendly)
// ---------------------------------------------------------------------------
class NPCSystem {
public:
    NPCSystem(WorldState& world, WorldMap& map, FactionRegistry& factions,
              EventBus& bus, uint64_t world_seed);

    // Must be called once after world is built
    // Build the BT factory in-place. Never pass a pre-built factory: BT.CPP
    // stores a back-reference to the factory object inside its XMLParser, so
    // any move/copy of the factory corrupts that reference.
    void initialize();

    // Per-tick update
    void tick(entt::registry& reg, uint64_t current_tick);

    // Spawn a new NPC at (x, y) for a faction with a given role
    entt::entity spawn(entt::registry& reg, int32_t x, int32_t y,
                       uint32_t faction_id, SocialRole role,
                       uint64_t current_tick);

    // Kill an NPC (deregisters from all systems)
    void kill(entt::registry& reg, entt::entity e, NPCDiedEvent::Cause cause,
              uint64_t current_tick);

private:
    void sync_blackboard(entt::registry& reg, entt::entity e,
                         NPCBlackboard& bb, uint64_t tick);
    void update_needs_decay(entt::registry& reg, uint64_t tick);
    void update_behavior_trees(entt::registry& reg, uint64_t tick);
    void update_state_machines(entt::registry& reg, uint64_t tick);
    void check_death_conditions(entt::registry& reg, uint64_t tick);
    void update_region_population_cache(entt::registry& reg);

    WorldState&       world_;
    WorldMap&         map_;
    FactionRegistry&  factions_;
    EventBus&         bus_;
    uint64_t          world_seed_;

    // BT::XMLParser stores a const-reference back to this factory object; the
    // factory must never be moved or copied after construction. NPCSystem is
    // heap-allocated (via unique_ptr in SimulationKernel), so this member is
    // at a stable address for its lifetime.
    BT::BehaviorTreeFactory bt_factory_;
    std::string             default_tree_xml_;

    // Scratch blackboard pool — one per entity, avoids per-tick alloc
    // keyed by entity value
    std::unordered_map<uint32_t, NPCBlackboard> bb_pool_;
};

}  // namespace somni

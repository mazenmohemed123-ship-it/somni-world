#pragma once
#include <somni/societies/Faction.hpp>
#include <cstdint>
#include <vector>

namespace somni {

class WorldState;
class EventBus;

// ---------------------------------------------------------------------------
// Active conflict between two factions
// ---------------------------------------------------------------------------
struct ActiveConflict {
    FactionID   attacker{FACTION_NONE};
    FactionID   defender{FACTION_NONE};
    uint32_t    theater_region{0};    // main contested region
    uint32_t    attacker_units{0};
    uint32_t    defender_units{0};
    uint64_t    started_tick{0};
    uint32_t    duration_ticks{0};
    bool        resolved{false};
    bool        attacker_won{false};
};

// ---------------------------------------------------------------------------
// Battle result — returned after a single combat resolution step
// ---------------------------------------------------------------------------
struct BattleResult {
    uint32_t attacker_losses{0};
    uint32_t defender_losses{0};
    bool     territory_changed{false};
    uint32_t new_controller{FACTION_NONE};
};

// ---------------------------------------------------------------------------
// Conflict system — deterministic combat resolution
// Uses Lanchester's square law + terrain modifiers + morale
// NO randomness — outcomes are seeded by world_seed XOR tick
// ---------------------------------------------------------------------------
class ConflictSystem {
public:
    ConflictSystem(WorldState& world, FactionRegistry& factions, EventBus& bus,
                   uint64_t world_seed);

    void tick(uint64_t current_tick);

    // Start a new conflict (called by DiplomacySystem or player)
    void start_conflict(FactionID attacker, FactionID defender, uint32_t region_id,
                        uint64_t current_tick);

    // Force end a conflict (peace treaty)
    void end_conflict(FactionID a, FactionID b);

    const std::vector<ActiveConflict>& active_conflicts() const { return conflicts_; }

private:
    // Lanchester's square law: Δstrength = α × enemy_strength²
    BattleResult resolve_battle_step(ActiveConflict& c, uint64_t tick);

    // Deterministic noise: seeded hash for battle randomness
    uint32_t deterministic_roll(uint64_t tick, FactionID a, FactionID b) const;

    void apply_territorial_change(ActiveConflict& c, bool attacker_wins, uint64_t tick);
    float terrain_modifier(uint32_t region_id, bool defending) const;
    float morale_modifier(const FactionData& f) const;

    WorldState&       world_;
    FactionRegistry&  factions_;
    EventBus&         bus_;
    uint64_t          world_seed_;
    std::vector<ActiveConflict> conflicts_;
};

// ---------------------------------------------------------------------------
// Diplomacy system — drives war/peace decisions based on faction state
// Runs once per N ticks (less frequently than combat)
// ---------------------------------------------------------------------------
class DiplomacySystem {
public:
    DiplomacySystem(FactionRegistry& factions, ConflictSystem& conflict, EventBus& bus,
                    uint64_t world_seed);

    void tick(uint64_t current_tick);

private:
    void evaluate_war_triggers(FactionID a, FactionID b, uint64_t tick);
    void evaluate_peace_triggers(FactionID a, FactionID b, uint64_t tick);
    float war_incentive(const FactionData& attacker, const FactionData& defender) const;

    FactionRegistry& factions_;
    ConflictSystem&  conflict_;
    EventBus&        bus_;
    uint64_t         world_seed_;

    static constexpr uint32_t EVAL_INTERVAL_TICKS = 200;
};

}  // namespace somni

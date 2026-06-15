#pragma once
#include <somni/core/DeterministicMath.hpp>
#include <array>
#include <cstdint>
#include <vector>

namespace somni {

class WorldState;
class FactionRegistry;

// ===========================================================================
// Hierarchical Simulation LOD
//
// Regions are simulated at different detail levels depending on distance
// from the observer (player / camera / focus point).
//
// FULL        (≤ 4 regions radius)  — every NPC runs BT + FSM each tick
// STATISTICAL (≤ 16 regions radius) — no individual NPCs; region-level
//                                     aggregates updated every N ticks
// FROZEN      (> 16 regions radius) — no updates; state preserved in place
// ===========================================================================

enum class SimLOD : uint8_t {
    FULL        = 0,
    STATISTICAL = 1,
    FROZEN      = 2,
};

// ---------------------------------------------------------------------------
// Thresholds (in region-grid distance units)
// ---------------------------------------------------------------------------
struct LODThresholds {
    float full_radius{4.0f};         // regions within this → FULL
    float statistical_radius{16.0f}; // regions within this → STATISTICAL
    // beyond statistical_radius      → FROZEN

    uint32_t stat_update_interval{10};  // STATISTICAL regions update every N ticks
};

// ---------------------------------------------------------------------------
// Per-region LOD state
// ---------------------------------------------------------------------------
struct RegionLODState {
    SimLOD   lod{SimLOD::FULL};
    float    dist_to_observer{0.0f};  // in region-grid cells
    uint64_t last_stat_tick{0};       // when statistical update last ran

    // Statistical population model (used when LOD >= STATISTICAL)
    // Updated using aggregated birth/death rates without individual NPC sim
    struct StatModel {
        Fixed32 population{0};          // total NPCs (fixed-point for determinism)
        Fixed32 birth_rate{0};          // births per tick per capita
        Fixed32 death_rate{0};          // deaths per tick per capita
        Fixed32 food_index{0};          // [0,1] food availability → affects death rate
        Fixed32 threat_index{0};        // [0,1] threat level → affects death rate
        Fixed32 resource_drain{0};      // resource consumption per tick

        // Advance population by n_ticks using closed-form logistic approximation
        void advance(uint32_t n_ticks) {
            if (population <= Fixed32::ZERO) return;
            Fixed32 net_rate = birth_rate - death_rate
                - threat_index * Fixed32{0.002f}
                - (food_index < Fixed32{0.3f} ? Fixed32{0.003f} : Fixed32::ZERO);
            // population(t) = population(0) * (1 + net_rate)^n_ticks
            // Approximated as: p *= (1 + net_rate * n_ticks) for small rates
            Fixed32 growth = Fixed32::ONE + net_rate * Fixed32(static_cast<int32_t>(n_ticks));
            population = Fixed32::max(Fixed32::ZERO, population * growth);
        }
    } stat_model;

    bool needs_stat_update(uint64_t current_tick, uint32_t interval) const {
        return lod == SimLOD::STATISTICAL &&
               (current_tick - last_stat_tick) >= interval;
    }
};

// ---------------------------------------------------------------------------
// LOD Manager — assigns and updates LOD for every region each tick
// ---------------------------------------------------------------------------
class LODManager {
public:
    LODManager(const WorldState& world, LODThresholds thresholds = {});

    // Update observer position (called when player moves or focus changes)
    void set_observer(int32_t region_gx, int32_t region_gy);

    // Recompute LOD for all regions (call once per tick or on observer move)
    void update_lod();

    // Statistical update for far regions (call every stat_update_interval ticks)
    void tick_statistical(uint64_t current_tick, FactionRegistry& factions);

    SimLOD   lod_for(uint32_t region_id) const;
    bool     is_full(uint32_t region_id)        const { return lod_for(region_id) == SimLOD::FULL; }
    bool     is_statistical(uint32_t region_id) const { return lod_for(region_id) == SimLOD::STATISTICAL; }
    bool     is_frozen(uint32_t region_id)      const { return lod_for(region_id) == SimLOD::FROZEN; }

    uint32_t count_full()        const;
    uint32_t count_statistical() const;
    uint32_t count_frozen()      const;

    const RegionLODState& state(uint32_t region_id) const {
        return states_[region_id];
    }

    // Initialize stat models from current ECS population (call after bootstrap)
    void initialize_stat_models(uint32_t total_npc_pop);

private:
    float region_distance(int32_t gx, int32_t gy) const;

    const WorldState&          world_;
    LODThresholds              thresholds_;
    std::vector<RegionLODState> states_;
    int32_t                    observer_gx_{0};
    int32_t                    observer_gy_{0};
};

// ---------------------------------------------------------------------------
// LOD-aware NPC skip gate
// Inline check used in NPCSystem to skip BT execution for far NPCs
// ---------------------------------------------------------------------------
inline bool should_simulate_full(const LODManager& lod, uint32_t region_id) {
    return lod.is_full(region_id);
}

}  // namespace somni

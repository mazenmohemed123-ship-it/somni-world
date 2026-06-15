#include <somni/core/SimulationLOD.hpp>
#include <somni/core/WorldState.hpp>
#include <somni/societies/Faction.hpp>
#include <cmath>
#include <algorithm>

namespace somni {

LODManager::LODManager(const WorldState& world, LODThresholds thresholds)
    : world_(world), thresholds_(thresholds) {
    states_.resize(world_.config().total_regions());
}

void LODManager::set_observer(int32_t region_gx, int32_t region_gy) {
    observer_gx_ = region_gx;
    observer_gy_ = region_gy;
    update_lod();
}

float LODManager::region_distance(int32_t gx, int32_t gy) const {
    float dx = static_cast<float>(gx - observer_gx_);
    float dy = static_cast<float>(gy - observer_gy_);
    return std::sqrt(dx * dx + dy * dy);
}

void LODManager::update_lod() {
    int32_t rw = world_.config().region_grid_w();
    uint32_t total = world_.config().total_regions();

    for (uint32_t rid = 0; rid < total; ++rid) {
        int32_t gx = static_cast<int32_t>(rid % rw);
        int32_t gy = static_cast<int32_t>(rid / rw);
        float dist = region_distance(gx, gy);

        auto& s = states_[rid];
        s.dist_to_observer = dist;

        if (dist <= thresholds_.full_radius) {
            s.lod = SimLOD::FULL;
        } else if (dist <= thresholds_.statistical_radius) {
            s.lod = SimLOD::STATISTICAL;
        } else {
            s.lod = SimLOD::FROZEN;
        }
    }
}

void LODManager::initialize_stat_models(uint32_t total_npc_pop) {
    uint32_t total = world_.config().total_regions();
    uint32_t stat_count = count_statistical();
    if (stat_count == 0) return;

    float pop_per_region = (stat_count > 0)
        ? static_cast<float>(total_npc_pop) / stat_count : 0.0f;

    for (uint32_t rid = 0; rid < total; ++rid) {
        auto& s = states_[rid];
        if (s.lod != SimLOD::STATISTICAL) continue;

        const auto& r = world_.region(rid);
        auto& sm = s.stat_model;

        sm.population    = Fixed32(static_cast<int32_t>(pop_per_region));
        sm.birth_rate    = Fixed32{0.0001f};
        sm.death_rate    = Fixed32{0.00008f};
        sm.food_index    = Fixed32(std::min(1.0f,
            r.resource_amount[0] / (r.resource_cap[0] + 1.0f)));
        sm.threat_index  = Fixed32{0.0f};
        sm.resource_drain = Fixed32(static_cast<int32_t>(pop_per_region) / 100);
    }
}

void LODManager::tick_statistical(uint64_t current_tick, FactionRegistry& factions) {
    uint32_t interval = thresholds_.stat_update_interval;

    for (uint32_t rid = 0; rid < states_.size(); ++rid) {
        auto& s = states_[rid];
        if (!s.needs_stat_update(current_tick, interval)) continue;

        uint32_t elapsed = static_cast<uint32_t>(current_tick - s.last_stat_tick);
        s.stat_model.advance(elapsed);
        s.last_stat_tick = current_tick;

        // Sync population back to region NPC count approximation
        auto& r = const_cast<RegionState&>(world_.region(rid));
        r.npc_count = static_cast<uint32_t>(std::max(0, s.stat_model.population.to_int()));

        // Drain resources proportional to statistical population
        if (s.stat_model.resource_drain > Fixed32::ZERO) {
            float drain = s.stat_model.resource_drain.to_float() * elapsed;
            r.resource_amount[0] = std::max(0.0f, r.resource_amount[0] - drain * 0.5f);
            r.resource_amount[3] = std::max(0.0f, r.resource_amount[3] - drain * 0.3f);
        }
    }
}

SimLOD LODManager::lod_for(uint32_t region_id) const {
    return states_[region_id].lod;
}

uint32_t LODManager::count_full() const {
    return static_cast<uint32_t>(
        std::count_if(states_.begin(), states_.end(),
                      [](const RegionLODState& s) { return s.lod == SimLOD::FULL; }));
}

uint32_t LODManager::count_statistical() const {
    return static_cast<uint32_t>(
        std::count_if(states_.begin(), states_.end(),
                      [](const RegionLODState& s) { return s.lod == SimLOD::STATISTICAL; }));
}

uint32_t LODManager::count_frozen() const {
    return static_cast<uint32_t>(
        std::count_if(states_.begin(), states_.end(),
                      [](const RegionLODState& s) { return s.lod == SimLOD::FROZEN; }));
}

}  // namespace somni

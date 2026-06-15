#include <somni/societies/Faction.hpp>
#include <algorithm>

namespace somni {

FactionID FactionRegistry::create_faction(FactionData data) {
    FactionID id = next_id_++;
    data.id = id;
    factions_[id] = std::move(data);
    return id;
}

void FactionRegistry::destroy_faction(FactionID id) {
    factions_.erase(id);
}

FactionData* FactionRegistry::get(FactionID id) {
    auto it = factions_.find(id);
    return it != factions_.end() ? &it->second : nullptr;
}

const FactionData* FactionRegistry::get(FactionID id) const {
    auto it = factions_.find(id);
    return it != factions_.end() ? &it->second : nullptr;
}

std::vector<FactionID> FactionRegistry::all_ids() const {
    std::vector<FactionID> ids;
    ids.reserve(factions_.size());
    for (const auto& [id, _] : factions_) ids.push_back(id);
    std::sort(ids.begin(), ids.end());
    return ids;
}

void FactionRegistry::tick(uint64_t current_tick) {
    for (auto& [id, fd] : factions_) {
        // Population growth
        fd.population = static_cast<uint32_t>(
            fd.population * (1.0f + fd.population_growth_rate));

        // Treasury update
        fd.treasury += fd.income_per_tick - fd.expenditure_per_tick;
        if (fd.treasury < 0.0f) {
            fd.stability -= 0.001f;   // debt hurts stability
            fd.treasury = 0.0f;
        }

        // Stability recovery / decay
        float target_stability = fd.treasury > 100.0f ? 0.85f : 0.5f;
        fd.stability += (target_stability - fd.stability) * 0.0001f;
        fd.stability  = std::clamp(fd.stability, 0.0f, 1.0f);

        // Military expenditure
        fd.expenditure_per_tick =
            static_cast<float>(fd.military_strength) * 0.01f;
        fd.income_per_tick =
            static_cast<float>(fd.territory_regions.size()) * 0.5f;
    }
}

}  // namespace somni

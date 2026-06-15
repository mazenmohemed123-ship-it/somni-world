#include <somni/societies/Conflict.hpp>
#include <somni/core/WorldState.hpp>
#include <somni/core/EventBus.hpp>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cmath>

namespace somni {

// ---------------------------------------------------------------------------
// ConflictSystem
// ---------------------------------------------------------------------------

ConflictSystem::ConflictSystem(WorldState& world, FactionRegistry& factions,
                                EventBus& bus, uint64_t world_seed)
    : world_(world), factions_(factions), bus_(bus), world_seed_(world_seed) {}

uint32_t ConflictSystem::deterministic_roll(uint64_t tick, FactionID a, FactionID b) const {
    uint64_t h = world_seed_
        ^ (tick          * 6364136223846793005ULL)
        ^ (uint64_t(a)   * 2654435761ULL)
        ^ (uint64_t(b)   * 40503ULL);
    h ^= h >> 33;
    h *= 0xFF51AFD7ED558CCDULL;
    h ^= h >> 33;
    return static_cast<uint32_t>(h & 0xFFFFFFFF);
}

float ConflictSystem::terrain_modifier(uint32_t region_id, bool defending) const {
    if (region_id >= world_.regions().size()) return 1.0f;
    Biome b = world_.region(region_id).dominant_biome;
    float mod = 1.0f;
    switch (b) {
        case Biome::MOUNTAINS:     mod = defending ? 1.5f : 0.7f; break;
        case Biome::JUNGLE:        mod = defending ? 1.3f : 0.8f; break;
        case Biome::SWAMP:         mod = 0.8f; break;
        case Biome::GRASSLAND:     mod = 1.0f; break;
        default:                   mod = 1.0f; break;
    }
    return mod;
}

float ConflictSystem::morale_modifier(const FactionData& f) const {
    return 0.5f + f.military_morale * 0.5f;
}

BattleResult ConflictSystem::resolve_battle_step(ActiveConflict& c, uint64_t tick) {
    BattleResult result;
    auto* atk = factions_.get(c.attacker);
    auto* def = factions_.get(c.defender);
    if (!atk || !def) { result.attacker_losses = c.attacker_units; return result; }

    // Lanchester's Square Law: dA/dt = -β * D^2 / A (simplified)
    float atk_power = static_cast<float>(c.attacker_units)
        * morale_modifier(*atk)
        * terrain_modifier(c.theater_region, false);
    float def_power = static_cast<float>(c.defender_units)
        * morale_modifier(*def)
        * terrain_modifier(c.theater_region, true);

    // Deterministic noise ±10%
    uint32_t roll = deterministic_roll(tick, c.attacker, c.defender);
    float noise   = 0.9f + (roll % 200) / 1000.0f;  // [0.90, 1.09]

    float atk_eff = atk_power * noise;
    float def_eff = def_power * (2.0f - noise);

    // Casualties per step = 2% of effective opposing strength
    uint32_t atk_loss = static_cast<uint32_t>(def_eff * 0.02f);
    uint32_t def_loss = static_cast<uint32_t>(atk_eff * 0.02f);

    atk_loss = std::min(atk_loss, c.attacker_units);
    def_loss = std::min(def_loss, c.defender_units);

    c.attacker_units -= atk_loss;
    c.defender_units -= def_loss;
    atk->military_strength = std::max(0u, atk->military_strength - atk_loss);
    def->military_strength = std::max(0u, def->military_strength - def_loss);

    result.attacker_losses = atk_loss;
    result.defender_losses = def_loss;

    spdlog::debug("Battle tick={} atk={} def={} atk_units={} def_units={} "
                  "atk_loss={} def_loss={}",
                  tick, c.attacker, c.defender, c.attacker_units, c.defender_units,
                  atk_loss, def_loss);
    return result;
}

void ConflictSystem::apply_territorial_change(ActiveConflict& c, bool attacker_wins,
                                               uint64_t tick) {
    auto& region = world_.region(c.theater_region);
    if (attacker_wins) {
        // Transfer territory
        auto* def = factions_.get(c.defender);
        auto* atk = factions_.get(c.attacker);
        if (def) def->territory_regions.erase(c.theater_region);
        if (atk) atk->territory_regions.insert(c.theater_region);
        region.controlling_faction = c.attacker;
        region.control_strength    = 0.5f;

        factions_.diplomacy().adjust_relation(c.attacker, c.defender, -0.2f);
    } else {
        region.control_strength = std::min(1.0f, region.control_strength + 0.1f);
    }
}

void ConflictSystem::tick(uint64_t current_tick) {
    for (auto& c : conflicts_) {
        if (c.resolved) continue;

        auto result = resolve_battle_step(c, current_tick);
        ++c.duration_ticks;

        bool atk_destroyed = c.attacker_units == 0;
        bool def_destroyed = c.defender_units == 0;

        if (atk_destroyed || def_destroyed || c.duration_ticks > 5000) {
            c.resolved     = true;
            c.attacker_won = def_destroyed && !atk_destroyed;
            apply_territorial_change(c, c.attacker_won, current_tick);
            factions_.diplomacy().sign_peace(c.attacker, c.defender);

            BattleResolvedEvent ev;
            ev.attacker_faction = c.attacker;
            ev.defender_faction = c.defender;
            ev.region_id        = c.theater_region;
            ev.attacker_losses  = result.attacker_losses;
            ev.defender_losses  = result.defender_losses;
            ev.attacker_won     = c.attacker_won;
            bus_.emit(ev);

            spdlog::info("Battle resolved: atk={} def={} won={} region={}",
                         c.attacker, c.defender, c.attacker_won, c.theater_region);
        }
    }

    // Remove resolved conflicts
    conflicts_.erase(std::remove_if(conflicts_.begin(), conflicts_.end(),
                                    [](const ActiveConflict& c) { return c.resolved; }),
                     conflicts_.end());
}

void ConflictSystem::start_conflict(FactionID attacker, FactionID defender,
                                    uint32_t region_id, uint64_t current_tick) {
    // Don't double-start
    for (const auto& c : conflicts_) {
        if ((c.attacker == attacker && c.defender == defender) ||
            (c.attacker == defender && c.defender == attacker)) return;
    }

    auto* atk = factions_.get(attacker);
    auto* def = factions_.get(defender);
    if (!atk || !def) return;

    ActiveConflict c;
    c.attacker        = attacker;
    c.defender        = defender;
    c.theater_region  = region_id;
    c.attacker_units  = atk->military_strength;
    c.defender_units  = def->military_strength;
    c.started_tick    = current_tick;
    conflicts_.push_back(c);

    factions_.diplomacy().declare_war(attacker, defender);

    FactionWarStartEvent ev;
    ev.attacker_id    = attacker;
    ev.defender_id    = defender;
    ev.trigger_region = region_id;
    bus_.emit(ev);

    spdlog::info("War started: atk={} def={} region={}", attacker, defender, region_id);
}

void ConflictSystem::end_conflict(FactionID a, FactionID b) {
    for (auto& c : conflicts_) {
        if ((c.attacker == a && c.defender == b) ||
            (c.attacker == b && c.defender == a)) {
            c.resolved = true;
        }
    }
    factions_.diplomacy().sign_peace(a, b);
}

// ---------------------------------------------------------------------------
// DiplomacySystem
// ---------------------------------------------------------------------------

DiplomacySystem::DiplomacySystem(FactionRegistry& factions, ConflictSystem& conflict,
                                  EventBus& bus, uint64_t world_seed)
    : factions_(factions), conflict_(conflict), bus_(bus), world_seed_(world_seed) {}

float DiplomacySystem::war_incentive(const FactionData& atk, const FactionData& def) const {
    float incentive = 0.0f;
    // Stronger attacks weaker
    if (atk.military_strength > def.military_strength * 1.5f) incentive += 0.3f;
    // Resource scarcity
    if (atk.stockpile[0] < 10.0f) incentive += 0.2f;  // food shortage
    // Territory greed
    incentive += static_cast<float>(def.territory_regions.size()) * 0.01f;
    // Hostility threshold
    return incentive;
}

void DiplomacySystem::evaluate_war_triggers(FactionID a, FactionID b, uint64_t tick) {
    auto* fa = factions_.get(a);
    auto* fb = factions_.get(b);
    if (!fa || !fb) return;

    const auto& rel = factions_.diplomacy().get(a, b);
    if (rel.at_war) return;

    float incentive = war_incentive(*fa, *fb);
    float hostility = -rel.value;

    // Deterministic threshold check
    uint64_t h = world_seed_ ^ (tick * 1000003ULL) ^ (uint64_t(a) * 7919ULL) ^ uint64_t(b);
    float threshold = 0.5f + (h & 0xFF) / 510.0f;  // [0.5, 1.0]

    if (hostility > 0.5f && incentive > threshold) {
        // Find contested border region
        uint32_t contested = 0;
        bool found = false;
        for (uint32_t rid : fa->territory_regions) {
            if (!found) { contested = rid; found = true; }
        }
        if (!found) return;
        conflict_.start_conflict(a, b, contested, tick);
    }
}

void DiplomacySystem::evaluate_peace_triggers(FactionID a, FactionID b, uint64_t tick) {
    const auto& rel = factions_.diplomacy().get(a, b);
    if (!rel.at_war) return;

    auto* fa = factions_.get(a);
    auto* fb = factions_.get(b);
    if (!fa || !fb) return;

    // Peace if one side is too weak to fight
    if (fa->military_strength < 5 || fb->military_strength < 5) {
        conflict_.end_conflict(a, b);
        FactionPeaceEvent ev; ev.faction_a = a; ev.faction_b = b;
        bus_.emit(ev);
    }
}

void DiplomacySystem::tick(uint64_t current_tick) {
    if (current_tick % EVAL_INTERVAL_TICKS != 0) return;

    auto ids = factions_.all_ids();
    for (size_t i = 0; i < ids.size(); ++i) {
        for (size_t j = i + 1; j < ids.size(); ++j) {
            evaluate_war_triggers(ids[i], ids[j], current_tick);
            evaluate_peace_triggers(ids[i], ids[j], current_tick);

            // Passive relation drift toward neutral
            factions_.diplomacy().adjust_relation(ids[i], ids[j], 0.00005f);
        }
    }
}

}  // namespace somni

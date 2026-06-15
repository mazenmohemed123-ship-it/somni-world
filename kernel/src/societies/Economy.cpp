#include <somni/societies/Economy.hpp>
#include <somni/core/WorldState.hpp>
#include <somni/core/EventBus.hpp>
#include <spdlog/spdlog.h>
#include <algorithm>

namespace somni {

EconomySystem::EconomySystem(WorldState& world, FactionRegistry& factions, EventBus& bus)
    : world_(world), factions_(factions), bus_(bus) {
    uint32_t n = world_.config().total_regions();
    market_.resize(n);
    for (uint32_t i = 0; i < n; ++i) {
        for (uint8_t r = 0; r < RESOURCE_COUNT; ++r) {
            market_[i][r].supply = world_.region(i).resource_amount[r];
        }
    }
}

void EconomySystem::tick(uint64_t current_tick) {
    update_production(current_tick);
    update_demand();
    update_prices();
    execute_trades(current_tick);
    balance_faction_budgets(current_tick);
}

void EconomySystem::update_production(uint64_t /*tick*/) {
    for (uint32_t rid = 0; rid < world_.regions().size(); ++rid) {
        const auto& region = world_.region(rid);
        for (uint8_t r = 0; r < RESOURCE_COUNT; ++r) {
            market_[rid][r].supply = region.resource_amount[r];
        }
    }
}

void EconomySystem::update_demand() {
    // Demand = npc_count * consumption rate per resource
    static const float DEMAND_PER_NPC[RESOURCE_COUNT] = {
        0.002f,  // FOOD_GRAIN
        0.001f,  // FOOD_MEAT
        0.001f,  // FOOD_FISH
        0.003f,  // WATER_FRESH
        0.0005f, // HERBS
        0.001f,  // TIMBER
        0.0005f, // STONE
        0.0003f, // IRON_ORE
        0.0001f, // GOLD
        0.0002f, // COAL
        0.0002f, // SALT
        0.0003f, // TOOLS
        0.0001f, // WEAPONS
        0.0003f, // TEXTILES
        0.0f,    // CURRENCY
        0.0f,    // MANA
    };

    for (uint32_t rid = 0; rid < world_.regions().size(); ++rid) {
        float pop = static_cast<float>(world_.region(rid).npc_count);
        for (uint8_t r = 0; r < RESOURCE_COUNT; ++r) {
            market_[rid][r].demand = pop * DEMAND_PER_NPC[r];
        }
    }
}

void EconomySystem::update_prices() {
    for (auto& market_row : market_) {
        for (auto& cell : market_row) {
            cell.update_price();
        }
    }
}

void EconomySystem::execute_trades(uint64_t current_tick) {
    for (auto& route : routes_) {
        if (!route.active) continue;
        if (route.from_region >= world_.regions().size() ||
            route.to_region  >= world_.regions().size()) continue;

        auto& from = world_.region(route.from_region);
        auto& to   = world_.region(route.to_region);
        uint8_t rt = static_cast<uint8_t>(route.resource);

        float available = from.resource_amount[rt];
        float volume    = std::min(route.volume_per_tick, available * 0.5f);
        if (volume < 0.01f) continue;

        // Check price incentive: only trade if destination price is higher
        float from_price = market_[route.from_region][rt].price_index;
        float to_price   = market_[route.to_region][rt].price_index;
        if (to_price <= from_price * 1.1f) continue;  // not worth it

        from.resource_amount[rt] -= volume;
        to.resource_amount[rt]    = std::min(
            to.resource_amount[rt] + volume, to.resource_cap[rt]);

        // Toll payment
        float toll = volume * route.toll_rate;
        auto* ctrl_faction = factions_.get(to.controlling_faction);
        if (ctrl_faction) ctrl_faction->treasury += toll;

        // Relation boost from trade
        if (route.faction_a != FACTION_NONE && route.faction_b != FACTION_NONE) {
            factions_.diplomacy().adjust_relation(
                route.faction_a, route.faction_b, 0.0001f);
        }

        TradeCompletedEvent ev;
        ev.faction_a   = route.faction_a;
        ev.faction_b   = route.faction_b;
        ev.resource_type = rt;
        ev.amount      = volume;
        ev.price       = to_price;
        bus_.emit(ev);
    }
}

void EconomySystem::balance_faction_budgets(uint64_t /*tick*/) {
    for (FactionID fid : factions_.all_ids()) {
        auto* fd = factions_.get(fid);
        if (!fd) continue;

        // Compute aggregate stockpile from controlled regions
        fd->stockpile.fill(0.0f);
        for (uint32_t rid : fd->territory_regions) {
            if (rid >= world_.regions().size()) continue;
            const auto& r = world_.region(rid);
            for (uint8_t i = 0; i < 16; ++i)
                fd->stockpile[i] += r.resource_amount[i];
        }
    }
}

void EconomySystem::add_trade_route(TradeRoute route) {
    routes_.push_back(std::move(route));
}

void EconomySystem::remove_trade_route(uint32_t from, uint32_t to, ResourceType r) {
    routes_.erase(
        std::remove_if(routes_.begin(), routes_.end(),
            [from, to, r](const TradeRoute& rt) {
                return rt.from_region == from && rt.to_region == to && rt.resource == r;
            }),
        routes_.end());
}

bool EconomySystem::transfer(FactionID from, FactionID to, ResourceType r, float amount) {
    auto* fd_from = factions_.get(from);
    auto* fd_to   = factions_.get(to);
    if (!fd_from || !fd_to) return false;

    uint8_t ri = static_cast<uint8_t>(r);
    if (fd_from->stockpile[ri] < amount) return false;
    fd_from->stockpile[ri] -= amount;
    fd_to->stockpile[ri]   += amount;
    return true;
}

}  // namespace somni

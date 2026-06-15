#pragma once
#include <somni/world/Resources.hpp>
#include <somni/societies/Faction.hpp>
#include <array>
#include <cstdint>
#include <vector>

namespace somni {

class WorldState;
class EventBus;

// ---------------------------------------------------------------------------
// Market price per resource per region — supply/demand driven, no currency fiction
// ---------------------------------------------------------------------------
struct MarketCell {
    float supply{0.0f};          // available amount
    float demand{0.0f};          // aggregate consumption rate
    float price_index{1.0f};     // relative price [0.1, 10.0]

    void update_price() {
        if (supply < 0.001f) { price_index = 10.0f; return; }
        float ratio = demand / (supply + 0.001f);
        price_index = std::clamp(ratio, 0.1f, 10.0f);
    }
};

// ---------------------------------------------------------------------------
// Trade route between two regions
// ---------------------------------------------------------------------------
struct TradeRoute {
    uint32_t     from_region{0};
    uint32_t     to_region{0};
    uint32_t     faction_a{0xFFFFFFFF};
    uint32_t     faction_b{0xFFFFFFFF};
    ResourceType resource{ResourceType::FOOD_GRAIN};
    float        volume_per_tick{0.0f};
    float        toll_rate{0.05f};   // fraction paid to controlling faction
    bool         active{true};
    uint64_t     established_tick{0};
};

// ---------------------------------------------------------------------------
// Production record — what a region produces each tick
// ---------------------------------------------------------------------------
struct ProductionRecord {
    uint32_t region_id{0};
    std::array<float, RESOURCE_COUNT> produced{};
    std::array<float, RESOURCE_COUNT> consumed{};
};

// ---------------------------------------------------------------------------
// Economy system — runs every tick
// Updates supply/demand, executes trades, adjusts prices
// ---------------------------------------------------------------------------
class EconomySystem {
public:
    EconomySystem(WorldState& world, FactionRegistry& factions, EventBus& bus);

    void tick(uint64_t current_tick);

    // Query
    const MarketCell& market(uint32_t region_id, ResourceType r) const {
        return market_[region_id][static_cast<uint8_t>(r)];
    }

    // Manually add a trade route (called at world bootstrap or from player)
    void add_trade_route(TradeRoute route);
    void remove_trade_route(uint32_t from, uint32_t to, ResourceType r);

    // Faction-level resource transfer
    bool transfer(FactionID from, FactionID to, ResourceType r, float amount);

    std::vector<TradeRoute>& routes() { return routes_; }

private:
    void update_production(uint64_t tick);
    void update_demand();
    void update_prices();
    void execute_trades(uint64_t tick);
    void balance_faction_budgets(uint64_t tick);

    WorldState&       world_;
    FactionRegistry&  factions_;
    EventBus&         bus_;

    // market_[region_id][resource_type]
    std::vector<std::array<MarketCell, RESOURCE_COUNT>> market_;
    std::vector<TradeRoute>     routes_;
    std::vector<ProductionRecord> production_cache_;
};

}  // namespace somni

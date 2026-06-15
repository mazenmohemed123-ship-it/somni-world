#pragma once
#include <somni/core/EventBus.hpp>

// ===========================================================================
// Segmented Event Buses
//
// Three independent buses prevent cross-domain coupling and allow
// subsystems to subscribe only to what they care about.
//
// WORLD BUS   — terrain, disasters, resources, environmental changes
// AGENT BUS   — NPC birth / death / state transitions / combat
// ECONOMY BUS — trades, market prices, faction budget, treasury
//
// Kernel owns SimulationBuses; subsystems receive a reference to their bus.
// ===========================================================================

namespace somni {

// ---------------------------------------------------------------------------
// World-domain events  (use world_bus)
// ---------------------------------------------------------------------------
struct TerrainChangedEvent : Event {
    uint32_t region_id;
    uint8_t  change_type;  // 0=deforestation, 1=flood, 2=fire, 3=earthquake
    float    severity;
};

struct DisasterEvent : Event {
    uint32_t region_id;
    uint8_t  disaster_type; // 0=famine, 1=flood, 2=plague, 3=earthquake
    float    magnitude;
    uint64_t tick;
};

struct ClimateShiftEvent : Event {
    uint8_t  new_season;
    float    temperature_delta;
    float    moisture_delta;
};

struct ResourceDiscoveryEvent : Event {
    uint32_t region_id;
    uint8_t  resource_type;
    float    amount_discovered;
    uint32_t discovering_faction;
};

// ---------------------------------------------------------------------------
// Agent-domain events  (use agent_bus)
// ---------------------------------------------------------------------------
struct NPCStateChangedEvent : Event {
    uint32_t npc_id;
    uint8_t  old_state;
    uint8_t  new_state;
    uint64_t tick;
};

struct NPCRecruitedEvent : Event {
    uint32_t npc_id;
    uint32_t recruiting_faction;
    uint8_t  new_role;
};

struct NPCLevelUpEvent : Event {
    uint32_t npc_id;
    uint8_t  skill_type;  // 0=combat, 1=farming, 2=trade, 3=crafting
    float    new_level;
};

struct CombatStartedEvent : Event {
    uint32_t attacker_npc;
    uint32_t defender_npc;
    uint32_t region_id;
};

// ---------------------------------------------------------------------------
// Economy-domain events  (use economy_bus)
// ---------------------------------------------------------------------------
struct MarketPriceChangedEvent : Event {
    uint32_t region_id;
    uint8_t  resource_type;
    float    old_price;
    float    new_price;
};

struct FactionBankruptcyEvent : Event {
    uint32_t faction_id;
    float    debt_at_collapse;
    uint64_t tick;
};

struct TradeRouteEstablishedEvent : Event {
    uint32_t from_region;
    uint32_t to_region;
    uint32_t faction_a;
    uint32_t faction_b;
    uint8_t  resource_type;
};

struct TradeRouteCollapsedEvent : Event {
    uint32_t from_region;
    uint32_t to_region;
    uint8_t  reason;  // 0=war, 1=resource_depleted, 2=faction_collapsed
};

struct FactionTreasuryUpdateEvent : Event {
    uint32_t faction_id;
    float    old_treasury;
    float    new_treasury;
    float    delta;
};

// ---------------------------------------------------------------------------
// SimulationBuses — owns all three buses
// ---------------------------------------------------------------------------
struct SimulationBuses {
    EventBus world;    // terrain / disasters / resources / environment
    EventBus agents;   // NPC birth / death / state / combat
    EventBus economy;  // trade / markets / faction finance

    // Broadcast a world-layer event
    template<typename E>
    void emit_world(const E& e)   { world.emit(e); }

    // Broadcast an agent-layer event
    template<typename E>
    void emit_agent(const E& e)   { agents.emit(e); }

    // Broadcast an economy-layer event
    template<typename E>
    void emit_economy(const E& e) { economy.emit(e); }

    // Flush: clear all handlers (used on world reset)
    void clear_all() {
        world.clear();
        agents.clear();
        economy.clear();
    }
};

}  // namespace somni

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <somni/core/SimulationKernel.hpp>
#include <somni/core/EventBus.hpp>
#include <somni/agents/Needs.hpp>
#include <somni/agents/Memory.hpp>
#include <somni/world/Resources.hpp>

using namespace somni;

// ---------------------------------------------------------------------------
// EventBus tests
// ---------------------------------------------------------------------------

TEST_CASE("EventBus emits and receives events", "[eventbus]") {
    EventBus bus;
    int count = 0;

    auto id = bus.subscribe<TickEvent>([&](const TickEvent& e) {
        ++count;
        REQUIRE(e.tick == 42);
    });

    bus.emit(TickEvent{42, 0.0});
    REQUIRE(count == 1);

    bus.unsubscribe(id);
    bus.emit(TickEvent{43, 0.0});
    REQUIRE(count == 1);  // handler removed
}

TEST_CASE("EventBus multiple event types", "[eventbus]") {
    EventBus bus;
    int tick_count = 0, npc_count = 0;

    bus.subscribe<TickEvent>([&](const TickEvent&)    { ++tick_count; });
    bus.subscribe<NPCDiedEvent>([&](const NPCDiedEvent&) { ++npc_count; });

    bus.emit(TickEvent{1, 0.0});
    bus.emit(TickEvent{2, 0.0});
    bus.emit(NPCDiedEvent{99, NPCDiedEvent::STARVATION, 0});

    REQUIRE(tick_count == 2);
    REQUIRE(npc_count  == 1);
}

// ---------------------------------------------------------------------------
// NeedsComponent tests
// ---------------------------------------------------------------------------

TEST_CASE("NeedsComponent initializes to 1.0", "[needs]") {
    NeedsComponent n;
    for (uint8_t i = 0; i < NEED_COUNT; ++i) {
        REQUIRE(n.values[i] == Catch::Approx(1.0f));
    }
}

TEST_CASE("NeedsComponent clamps values", "[needs]") {
    NeedsComponent n;
    n.set(NeedType::HUNGER, -0.5f);
    REQUIRE(n.get(NeedType::HUNGER) == Catch::Approx(0.0f));
    n.set(NeedType::HUNGER, 1.5f);
    REQUIRE(n.get(NeedType::HUNGER) == Catch::Approx(1.0f));
}

TEST_CASE("NeedsComponent critical detection", "[needs]") {
    NeedsComponent n;
    n.set(NeedType::HUNGER, 0.10f);
    REQUIRE(n.critical(NeedType::HUNGER) == true);
    n.set(NeedType::HUNGER, 0.50f);
    REQUIRE(n.critical(NeedType::HUNGER) == false);
}

TEST_CASE("NeedsUtility urgency scoring", "[needs]") {
    NeedsComponent n;
    n.set(NeedType::HUNGER, 0.0f);
    float s0 = NeedsUtility::hunger_score(n);
    n.set(NeedType::HUNGER, 0.5f);
    float s1 = NeedsUtility::hunger_score(n);
    n.set(NeedType::HUNGER, 1.0f);
    float s2 = NeedsUtility::hunger_score(n);
    // Urgency should decrease as hunger is satisfied
    REQUIRE(s0 > s1);
    REQUIRE(s1 > s2);
    REQUIRE(s2 == Catch::Approx(0.0f));
}

// ---------------------------------------------------------------------------
// MemoryComponent tests
// ---------------------------------------------------------------------------

TEST_CASE("MemoryComponent recall returns best-confidence entry", "[memory]") {
    MemoryComponent mem;
    mem.remember({10, 20, MemoryTag::FOOD_SOURCE, 100.0f, 0.5f, 1, 0});
    mem.remember({30, 40, MemoryTag::FOOD_SOURCE, 200.0f, 0.9f, 2, 0});
    mem.remember({50, 60, MemoryTag::WATER_SOURCE, 50.0f, 0.8f, 3, 0});

    auto food = mem.recall(MemoryTag::FOOD_SOURCE);
    REQUIRE(food.has_value());
    REQUIRE(food->world_x == 30);   // highest confidence
    REQUIRE(food->world_y == 40);

    auto water = mem.recall(MemoryTag::WATER_SOURCE);
    REQUIRE(water.has_value());
    REQUIRE(water->world_x == 50);
}

TEST_CASE("MemoryComponent evicts oldest on overflow", "[memory]") {
    MemoryComponent mem;
    for (int i = 0; i < MemoryComponent::CAPACITY + 5; ++i) {
        mem.remember({i, i, MemoryTag::FOOD_SOURCE, 1.0f, 1.0f, static_cast<uint64_t>(i), 0});
    }
    REQUIRE(mem.size == MemoryComponent::CAPACITY);
}

TEST_CASE("MemoryComponent confidence decay", "[memory]") {
    MemoryComponent mem;
    mem.remember({0, 0, MemoryTag::THREAT, 1.0f, 1.0f, 0, 0});
    mem.decay(0.1f);
    REQUIRE(mem.entries[0].confidence == Catch::Approx(0.9f));
    mem.decay(0.9f);
    REQUIRE(mem.entries[0].confidence == Catch::Approx(0.0f));
    REQUIRE(!mem.entries[0].valid());
}

// ---------------------------------------------------------------------------
// InventoryComponent tests
// ---------------------------------------------------------------------------

TEST_CASE("InventoryComponent add and remove", "[inventory]") {
    InventoryComponent inv;
    REQUIRE(inv.add(ResourceType::FOOD_GRAIN, 10.0f));
    REQUIRE(inv.has(ResourceType::FOOD_GRAIN));
    REQUIRE(inv.get(ResourceType::FOOD_GRAIN) == Catch::Approx(10.0f));

    float removed = inv.remove(ResourceType::FOOD_GRAIN, 3.0f);
    REQUIRE(removed == Catch::Approx(3.0f));
    REQUIRE(inv.get(ResourceType::FOOD_GRAIN) == Catch::Approx(7.0f));
}

TEST_CASE("InventoryComponent full returns false on add", "[inventory]") {
    InventoryComponent inv;
    for (uint8_t i = 0; i < InventoryComponent::SLOTS; ++i) {
        REQUIRE(inv.add(static_cast<ResourceType>(i), 1.0f));
    }
    REQUIRE(!inv.add(ResourceType::MANA, 1.0f));
}

// ---------------------------------------------------------------------------
// WorldState tests
// ---------------------------------------------------------------------------

TEST_CASE("WorldState clock advances correctly", "[worldstate]") {
    WorldClock clock;
    clock.advance(WorldClock::TICKS_PER_HOUR);
    REQUIRE(clock.hour == 7);   // started at 6
    REQUIRE(clock.day  == 1);

    clock.advance(WorldClock::TICKS_PER_DAY * 29);
    REQUIRE(clock.day   == 30);
    REQUIRE(clock.month == 1);
}

TEST_CASE("WorldState region_id_at boundary", "[worldstate]") {
    WorldConfig cfg;
    cfg.width = 64; cfg.height = 64; cfg.region_size = 16;
    WorldState ws(cfg);
    REQUIRE(ws.region_id_at(0, 0) == 0);
    REQUIRE(ws.region_id_at(15, 15) == 0);
    REQUIRE(ws.region_id_at(16, 0)  == 1);
    REQUIRE(ws.region_id_at(0, 16)  == 4);  // next row: 64/16 = 4 regions wide
}

TEST_CASE("WorldState resource regen stays within cap", "[worldstate]") {
    WorldConfig cfg;
    cfg.width = 64; cfg.height = 64; cfg.region_size = 16;
    WorldState ws(cfg);

    auto& r = ws.region(0);
    r.resource_amount[0] = r.resource_cap[0];  // at cap
    r.resource_regen[0]  = 10.0f;

    ws.tick_resources();
    REQUIRE(r.resource_amount[0] <= r.resource_cap[0]);
}

// ---------------------------------------------------------------------------
// Full kernel smoke test
// ---------------------------------------------------------------------------

TEST_CASE("SimulationKernel bootstrap and tick", "[kernel][integration]") {
    KernelConfig cfg;
    cfg.tick.headless         = true;
    cfg.tick.ticks_per_second = 100;
    cfg.snapshot_interval_ticks = 0;

    SimulationKernel kernel(cfg);

    WorldBootstrap spec;
    spec.world_config.seed   = 12345;
    spec.world_config.width  = 64;
    spec.world_config.height = 64;
    spec.world_config.name   = "test_world";
    spec.initial_npc_per_faction = 5;

    WorldBootstrap::FactionSpec fs;
    fs.name               = "TestFaction";
    fs.ideology           = FactionIdeology::TRIBAL;
    fs.government         = GovernmentType::CHIEFTAINCY;
    fs.spawn_x            = 32;
    fs.spawn_y            = 32;
    fs.initial_population = 10;
    fs.initial_treasury   = 100.0f;
    spec.factions.push_back(fs);

    REQUIRE_NOTHROW(kernel.bootstrap(spec));
    REQUIRE(!kernel.is_running());

    // Run 100 ticks synchronously
    TickConfig tc;
    tc.headless         = true;
    tc.ticks_per_second = 1000;

    TickEngine engine(tc);
    bool world_ticked = false;
    engine.register_system({"test", 10, [&](uint64_t tick) {
        world_ticked = true;
    }});
    engine.step(100);
    REQUIRE(engine.current_tick() == 100);
    REQUIRE(world_ticked);
}

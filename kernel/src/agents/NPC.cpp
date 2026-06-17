#include <somni/agents/NPC.hpp>
#include <somni/core/WorldState.hpp>
#include <somni/world/WorldMap.hpp>
#include <somni/societies/Faction.hpp>
#include <somni/core/EventBus.hpp>
#include <spdlog/spdlog.h>
#include <algorithm>

namespace somni {

// ---------------------------------------------------------------------------
// NPCArchetype
// ---------------------------------------------------------------------------

NPCArchetype NPCArchetype::for_role(SocialRole r) {
    NPCArchetype a;
    a.role = r;
    a.lifespan_ticks = 86400 * 60;  // ~60 sim-days at 20 TPS

    switch (r) {
        case SocialRole::SOLDIER:
            a.combat.attack  = 8.0f;
            a.combat.defense = 6.0f;
            a.decay.rates[static_cast<uint8_t>(NeedType::HUNGER)] = 0.00025f;
            break;
        case SocialRole::FARMER:
            a.combat.attack  = 2.0f;
            a.combat.defense = 1.0f;
            a.decay.rates[static_cast<uint8_t>(NeedType::HUNGER)] = 0.00018f;
            break;
        case SocialRole::TRADER:
            a.combat.attack  = 3.0f;
            a.combat.defense = 2.0f;
            break;
        default:
            break;
    }
    return a;
}

// ---------------------------------------------------------------------------
// NPCSystem
// ---------------------------------------------------------------------------

NPCSystem::NPCSystem(WorldState& world, WorldMap& map, FactionRegistry& factions,
                     EventBus& bus, uint64_t world_seed)
    : world_(world), map_(map), factions_(factions), bus_(bus), world_seed_(world_seed) {}

void NPCSystem::initialize() {
    SomniTreeFactory::register_nodes(bt_factory_);
    default_tree_xml_ = SomniTreeFactory::default_npc_tree_xml();
    bt_factory_.registerBehaviorTreeFromText(default_tree_xml_);
    spdlog::info("NPCSystem initialized with BT factory");
}

entt::entity NPCSystem::spawn(entt::registry& reg, int32_t x, int32_t y,
                               uint32_t faction_id, SocialRole role, uint64_t tick) {
    auto e = reg.create();
    auto arch = NPCArchetype::for_role(role);

    reg.emplace<PositionComponent>(e, x, y, world_.region_id_at(x, y));
    reg.emplace<NeedsComponent>(e);
    reg.emplace<NeedsDecayComponent>(e, arch.decay);
    reg.emplace<MemoryComponent>(e);
    reg.emplace<StateMachineComponent>(e);
    reg.emplace<SocialRoleComponent>(e, role, faction_id);
    reg.emplace<FactionComponent>(e, faction_id, 0.8f);
    reg.emplace<CombatComponent>(e, arch.combat);
    reg.emplace<InventoryComponent>(e);
    reg.emplace<AgeComponent>(e, static_cast<uint32_t>(tick), arch.lifespan_ticks);

    // Pre-allocate blackboard slot
    uint32_t eid = static_cast<uint32_t>(entt::to_integral(e));
    auto& bb = bb_pool_[eid];
    bb.entity     = e;
    bb.faction_id = faction_id;
    bb.social_role = static_cast<uint8_t>(role);
    bb.world      = &world_;
    bb.map        = &map_;
    bb.factions   = &factions_;
    bb.registry   = &reg;
    bb.world_seed = world_seed_;

    // Build BT for this NPC
    auto& bt_comp = reg.emplace<BehaviorTreeComponent>(e);
    bt_comp.blackboard = BT::Blackboard::create();
    bt_comp.blackboard->set("_ctx", &bb);
    bt_comp.tree = bt_factory_.createTree("NPCDefault", bt_comp.blackboard);
    bt_comp.initialized = true;

    return e;
}

void NPCSystem::kill(entt::registry& reg, entt::entity e, NPCDiedEvent::Cause cause,
                     uint64_t tick) {
    if (!reg.valid(e)) return;
    auto* pos = reg.try_get<PositionComponent>(e);
    uint32_t region_id = pos ? pos->region_id : 0;

    NPCDiedEvent ev;
    ev.npc_id    = static_cast<uint32_t>(entt::to_integral(e));
    ev.cause     = cause;
    ev.region_id = region_id;
    bus_.emit(ev);

    bb_pool_.erase(static_cast<uint32_t>(entt::to_integral(e)));
    reg.destroy(e);
}

void NPCSystem::sync_blackboard(entt::registry& reg, entt::entity e,
                                 NPCBlackboard& bb, uint64_t tick) {
    bb.current_tick = tick;
    bb.is_night     = world_.clock().is_night();

    auto* needs = reg.try_get<NeedsComponent>(e);
    if (needs) {
        bb.hunger = needs->get(NeedType::HUNGER);
        bb.thirst = needs->get(NeedType::THIRST);
        bb.safety = needs->get(NeedType::SAFETY);
        bb.rest   = needs->get(NeedType::REST);
        bb.social = needs->get(NeedType::SOCIAL);
    }
}

void NPCSystem::update_needs_decay(entt::registry& reg, uint64_t tick) {
    auto view = reg.view<NeedsComponent, NeedsDecayComponent, MemoryComponent>();
    view.each([&](auto e, NeedsComponent& needs, NeedsDecayComponent& decay,
                  MemoryComponent& mem) {
        for (uint8_t i = 0; i < NEED_COUNT; ++i) {
            needs.delta(static_cast<NeedType>(i), -decay.rates[i]);
        }
        // Decay memory confidence
        mem.decay(0.0002f);
    });
}

void NPCSystem::update_behavior_trees(entt::registry& reg, uint64_t tick) {
    auto view = reg.view<BehaviorTreeComponent, CombatComponent>();
    view.each([&](auto e, BehaviorTreeComponent& bt_comp, CombatComponent& combat) {
        if (!combat.is_alive) return;
        if (!bt_comp.initialized) return;

        uint32_t eid = static_cast<uint32_t>(entt::to_integral(e));
        auto it = bb_pool_.find(eid);
        if (it == bb_pool_.end()) return;

        sync_blackboard(reg, e, it->second, tick);
        bt_comp.blackboard->set("_ctx", &it->second);
        bt_comp.tree.tickOnce();
    });
}

void NPCSystem::update_state_machines(entt::registry& reg, uint64_t tick) {
    auto view = reg.view<StateMachineComponent, NeedsComponent, CombatComponent>();
    view.each([&](auto e, StateMachineComponent& fsm, NeedsComponent& needs,
                  CombatComponent& combat) {
        if (!combat.is_alive) return;

        uint32_t eid = static_cast<uint32_t>(entt::to_integral(e));
        auto it = bb_pool_.find(eid);

        struct Ctx {
            float hunger, thirst, safety, rest;
            bool enemy_in_range, is_night;
        } ctx{};

        if (it != bb_pool_.end()) {
            ctx.hunger       = it->second.hunger;
            ctx.thirst       = it->second.thirst;
            ctx.safety       = it->second.safety;
            ctx.rest         = it->second.rest;
            ctx.enemy_in_range = it->second.enemy_in_range;
            ctx.is_night     = it->second.is_night;
        } else {
            ctx.hunger = needs.get(NeedType::HUNGER);
            ctx.thirst = needs.get(NeedType::THIRST);
            ctx.safety = needs.get(NeedType::SAFETY);
            ctx.rest   = needs.get(NeedType::REST);
        }

        StateMachine::evaluate(fsm, ctx, tick);
    });
}

void NPCSystem::check_death_conditions(entt::registry& reg, uint64_t tick) {
    std::vector<entt::entity> to_kill;

    reg.view<NeedsComponent, CombatComponent, AgeComponent>().each(
        [&](auto e, NeedsComponent& needs, CombatComponent& combat, AgeComponent& age) {
            if (!combat.is_alive) { to_kill.push_back(e); return; }
            if (needs.get(NeedType::HUNGER) <= 0.0f) {
                combat.is_alive = false;
                to_kill.push_back(e);
                return;
            }
            if (needs.get(NeedType::THIRST) <= 0.0f) {
                combat.is_alive = false;
                to_kill.push_back(e);
                return;
            }
            if (tick - age.birth_tick >= age.lifespan_ticks) {
                combat.is_alive = false;
                to_kill.push_back(e);
            }
        });

    for (auto e : to_kill) {
        auto* needs  = reg.try_get<NeedsComponent>(e);
        NPCDiedEvent::Cause cause = NPCDiedEvent::Cause::OLD_AGE;
        if (needs) {
            if (needs->get(NeedType::HUNGER) <= 0.0f) cause = NPCDiedEvent::Cause::STARVATION;
            else if (needs->get(NeedType::THIRST) <= 0.0f) cause = NPCDiedEvent::Cause::DEHYDRATION;
        }
        auto* combat = reg.try_get<CombatComponent>(e);
        if (combat && !combat->is_alive && combat->kills > 0) cause = NPCDiedEvent::Cause::COMBAT;
        kill(reg, e, cause, tick);
    }
}

void NPCSystem::update_region_population_cache(entt::registry& reg) {
    // Zero out
    for (auto& r : world_.regions()) r.npc_count = 0;

    reg.view<PositionComponent, CombatComponent>().each(
        [&](auto, PositionComponent& pos, CombatComponent& combat) {
            if (!combat.is_alive) return;
            if (pos.region_id < world_.regions().size())
                ++world_.region(pos.region_id).npc_count;
        });
}

void NPCSystem::tick(entt::registry& reg, uint64_t tick) {
    update_needs_decay(reg, tick);
    update_behavior_trees(reg, tick);
    update_state_machines(reg, tick);
    check_death_conditions(reg, tick);
    if (tick % 100 == 0)  // cache update every 100 ticks
        update_region_population_cache(reg);
}

}  // namespace somni

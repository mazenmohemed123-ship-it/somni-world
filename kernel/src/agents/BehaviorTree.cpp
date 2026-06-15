#include <somni/agents/BehaviorTree.hpp>
#include <somni/core/WorldState.hpp>
#include <somni/world/WorldMap.hpp>
#include <somni/world/Resources.hpp>
#include <somni/societies/Faction.hpp>
#include <entt/entt.hpp>
#include <spdlog/spdlog.h>
#include <cmath>

namespace somni {

// ---------------------------------------------------------------------------
// Helper: retrieve NPCBlackboard from BT::TreeNode config
// ---------------------------------------------------------------------------
static NPCBlackboard* get_ctx(BT::TreeNode* node) {
    auto bb = node->config().blackboard;
    if (!bb) return nullptr;
    auto* ptr = bb->getAny("_ctx");
    if (!ptr) return nullptr;
    return *ptr->castPtr<NPCBlackboard*>();
}

// ---------------------------------------------------------------------------
// Condition Nodes
// ---------------------------------------------------------------------------

BT::NodeStatus IsHungry::tick() {
    auto* ctx = get_ctx(this);
    if (!ctx) return BT::NodeStatus::FAILURE;
    float threshold = 0.25f;
    getInput("threshold", threshold);
    return ctx->hunger < threshold ? BT::NodeStatus::SUCCESS : BT::NodeStatus::FAILURE;
}

BT::NodeStatus IsThirsty::tick() {
    auto* ctx = get_ctx(this);
    if (!ctx) return BT::NodeStatus::FAILURE;
    float threshold = 0.15f;
    getInput("threshold", threshold);
    return ctx->thirst < threshold ? BT::NodeStatus::SUCCESS : BT::NodeStatus::FAILURE;
}

BT::NodeStatus IsUnsafe::tick() {
    auto* ctx = get_ctx(this);
    if (!ctx) return BT::NodeStatus::FAILURE;
    float threshold = 0.30f;
    getInput("threshold", threshold);
    return ctx->safety < threshold ? BT::NodeStatus::SUCCESS : BT::NodeStatus::FAILURE;
}

BT::NodeStatus IsTired::tick() {
    auto* ctx = get_ctx(this);
    if (!ctx) return BT::NodeStatus::FAILURE;
    float threshold = 0.15f;
    getInput("threshold", threshold);
    return ctx->rest < threshold ? BT::NodeStatus::SUCCESS : BT::NodeStatus::FAILURE;
}

BT::NodeStatus HasResourceInInventory::tick() {
    auto* ctx = get_ctx(this);
    if (!ctx || !ctx->registry) return BT::NodeStatus::FAILURE;
    if (!ctx->registry->valid(ctx->entity)) return BT::NodeStatus::FAILURE;

    int res_type = 0;
    float min_amount = 0.01f;
    getInput("resource_type", res_type);
    getInput("min_amount", min_amount);

    auto* inv = ctx->registry->try_get<InventoryComponent>(ctx->entity);
    if (!inv) return BT::NodeStatus::FAILURE;
    return inv->has(static_cast<ResourceType>(res_type), min_amount)
        ? BT::NodeStatus::SUCCESS : BT::NodeStatus::FAILURE;
}

BT::NodeStatus EnemyNearby::tick() {
    auto* ctx = get_ctx(this);
    if (!ctx) return BT::NodeStatus::FAILURE;
    return ctx->enemy_in_range ? BT::NodeStatus::SUCCESS : BT::NodeStatus::FAILURE;
}

// ---------------------------------------------------------------------------
// Action Nodes
// ---------------------------------------------------------------------------

BT::NodeStatus MoveToTarget::onStart() {
    auto* ctx = get_ctx(this);
    if (!ctx) return BT::NodeStatus::FAILURE;
    getInput("target_x", ctx->target_x);
    getInput("target_y", ctx->target_y);
    ctx->has_target = true;
    return BT::NodeStatus::RUNNING;
}

BT::NodeStatus MoveToTarget::onRunning() {
    auto* ctx = get_ctx(this);
    if (!ctx || !ctx->registry || !ctx->map) return BT::NodeStatus::FAILURE;
    if (!ctx->registry->valid(ctx->entity)) return BT::NodeStatus::FAILURE;

    auto* pos = ctx->registry->try_get<PositionComponent>(ctx->entity);
    if (!pos) return BT::NodeStatus::FAILURE;

    if (pos->x == ctx->target_x && pos->y == ctx->target_y)
        return BT::NodeStatus::SUCCESS;

    auto [nx, ny] = ctx->map->step_toward(pos->x, pos->y, ctx->target_x, ctx->target_y);
    pos->x = nx; pos->y = ny;

    // Update region
    if (ctx->world) {
        pos->region_id = ctx->world->region_id_at(nx, ny);
    }
    return BT::NodeStatus::RUNNING;
}

BT::NodeStatus GatherResource::tick() {
    auto* ctx = get_ctx(this);
    if (!ctx || !ctx->world || !ctx->registry) return BT::NodeStatus::FAILURE;
    if (!ctx->registry->valid(ctx->entity)) return BT::NodeStatus::FAILURE;

    int res_type = 0;
    float amount = 1.0f;
    getInput("resource_type", res_type);
    getInput("amount", amount);

    auto* pos = ctx->registry->try_get<PositionComponent>(ctx->entity);
    if (!pos) return BT::NodeStatus::FAILURE;

    auto& region = ctx->world->region(pos->region_id);
    uint8_t rt = static_cast<uint8_t>(res_type);
    float available = region.resource_amount[rt];
    float gathered  = std::min(available, amount);

    if (gathered < 0.01f) return BT::NodeStatus::FAILURE;

    region.resource_amount[rt] -= gathered;

    auto* inv = ctx->registry->try_get<InventoryComponent>(ctx->entity);
    if (!inv || !inv->add(static_cast<ResourceType>(res_type), gathered))
        return BT::NodeStatus::FAILURE;

    // Record in memory
    auto* mem = ctx->registry->try_get<MemoryComponent>(ctx->entity);
    if (mem) {
        MemoryTag mtag = (res_type == static_cast<int>(ResourceType::FOOD_GRAIN) ||
                          res_type == static_cast<int>(ResourceType::FOOD_MEAT))
            ? MemoryTag::FOOD_SOURCE : MemoryTag::WATER_SOURCE;
        mem->reinforce(pos->x, pos->y, mtag, available, ctx->current_tick);
    }

    return BT::NodeStatus::SUCCESS;
}

BT::NodeStatus ConsumeFood::tick() {
    auto* ctx = get_ctx(this);
    if (!ctx || !ctx->registry) return BT::NodeStatus::FAILURE;
    if (!ctx->registry->valid(ctx->entity)) return BT::NodeStatus::FAILURE;

    auto* inv  = ctx->registry->try_get<InventoryComponent>(ctx->entity);
    auto* needs = ctx->registry->try_get<NeedsComponent>(ctx->entity);
    if (!inv || !needs) return BT::NodeStatus::FAILURE;

    float removed = 0.0f;
    removed += inv->remove(ResourceType::FOOD_GRAIN, 0.5f);
    removed += inv->remove(ResourceType::FOOD_MEAT,  0.5f);
    removed += inv->remove(ResourceType::FOOD_FISH,  0.5f);
    if (removed < 0.01f) return BT::NodeStatus::FAILURE;

    needs->delta(NeedType::HUNGER, removed * 0.3f);
    return BT::NodeStatus::SUCCESS;
}

BT::NodeStatus ConsumeWater::tick() {
    auto* ctx = get_ctx(this);
    if (!ctx || !ctx->registry) return BT::NodeStatus::FAILURE;
    if (!ctx->registry->valid(ctx->entity)) return BT::NodeStatus::FAILURE;

    auto* inv   = ctx->registry->try_get<InventoryComponent>(ctx->entity);
    auto* needs = ctx->registry->try_get<NeedsComponent>(ctx->entity);
    if (!inv || !needs) return BT::NodeStatus::FAILURE;

    float removed = inv->remove(ResourceType::WATER_FRESH, 0.5f);
    if (removed < 0.01f) {
        // Drink from region if water available
        auto* pos = ctx->registry->try_get<PositionComponent>(ctx->entity);
        if (pos && ctx->world) {
            auto& region = ctx->world->region(pos->region_id);
            float avail = region.resource_amount[static_cast<uint8_t>(ResourceType::WATER_FRESH)];
            if (avail > 0.1f) {
                float taken = std::min(avail, 0.5f);
                region.resource_amount[static_cast<uint8_t>(ResourceType::WATER_FRESH)] -= taken;
                needs->delta(NeedType::THIRST, taken * 0.4f);
                return BT::NodeStatus::SUCCESS;
            }
        }
        return BT::NodeStatus::FAILURE;
    }
    needs->delta(NeedType::THIRST, removed * 0.4f);
    return BT::NodeStatus::SUCCESS;
}

BT::NodeStatus RecallResourceLocation::tick() {
    auto* ctx = get_ctx(this);
    if (!ctx || !ctx->registry) return BT::NodeStatus::FAILURE;
    if (!ctx->registry->valid(ctx->entity)) return BT::NodeStatus::FAILURE;

    int res_type = 0;
    getInput("resource_type", res_type);

    auto* mem = ctx->registry->try_get<MemoryComponent>(ctx->entity);
    if (!mem) return BT::NodeStatus::FAILURE;

    MemoryTag tag = (res_type == static_cast<int>(ResourceType::WATER_FRESH))
        ? MemoryTag::WATER_SOURCE : MemoryTag::FOOD_SOURCE;

    auto entry = mem->recall(tag);
    if (!entry) return BT::NodeStatus::FAILURE;

    setOutput("out_x", entry->world_x);
    setOutput("out_y", entry->world_y);
    return BT::NodeStatus::SUCCESS;
}

BT::NodeStatus FleeFromThreat::onStart() {
    auto* ctx = get_ctx(this);
    if (!ctx) return BT::NodeStatus::FAILURE;
    return BT::NodeStatus::RUNNING;
}

BT::NodeStatus FleeFromThreat::onRunning() {
    auto* ctx = get_ctx(this);
    if (!ctx || !ctx->registry || !ctx->map) return BT::NodeStatus::FAILURE;
    if (!ctx->registry->valid(ctx->entity)) return BT::NodeStatus::FAILURE;

    auto* pos = ctx->registry->try_get<PositionComponent>(ctx->entity);
    if (!pos) return BT::NodeStatus::FAILURE;

    // Move away from enemy if known
    if (ctx->registry->valid(ctx->enemy_entity)) {
        auto* epos = ctx->registry->try_get<PositionComponent>(ctx->enemy_entity);
        if (epos) {
            int32_t fx = pos->x + (pos->x - epos->x);
            int32_t fy = pos->y + (pos->y - epos->y);
            fx = std::clamp(fx, 0, ctx->world->config().width  - 1);
            fy = std::clamp(fy, 0, ctx->world->config().height - 1);
            auto [nx, ny] = ctx->map->step_toward(pos->x, pos->y, fx, fy);
            pos->x = nx; pos->y = ny;
            if (ctx->world) pos->region_id = ctx->world->region_id_at(nx, ny);
        }
    }

    auto* needs = ctx->registry->try_get<NeedsComponent>(ctx->entity);
    if (needs && needs->get(NeedType::SAFETY) > 0.7f) {
        auto* mem = ctx->registry->try_get<MemoryComponent>(ctx->entity);
        if (mem) mem->forget_threats();
        return BT::NodeStatus::SUCCESS;
    }
    return BT::NodeStatus::RUNNING;
}

BT::NodeStatus AttackEnemy::tick() {
    auto* ctx = get_ctx(this);
    if (!ctx || !ctx->registry) return BT::NodeStatus::FAILURE;
    if (!ctx->registry->valid(ctx->entity)) return BT::NodeStatus::FAILURE;
    if (!ctx->registry->valid(ctx->enemy_entity)) return BT::NodeStatus::FAILURE;

    auto* atk_combat = ctx->registry->try_get<CombatComponent>(ctx->entity);
    auto* def_combat = ctx->registry->try_get<CombatComponent>(ctx->enemy_entity);
    if (!atk_combat || !def_combat) return BT::NodeStatus::FAILURE;

    // Simple combat: attacker deals attack - defender.defense damage
    float damage = std::max(0.0f, atk_combat->attack - def_combat->defense * 0.5f);
    damage /= 100.0f;  // normalize to health units
    def_combat->health -= damage;

    if (def_combat->health <= 0.0f) {
        def_combat->is_alive = false;
        ++atk_combat->kills;
        ctx->enemy_entity  = entt::null;
        ctx->enemy_in_range = false;
        return BT::NodeStatus::SUCCESS;
    }
    return BT::NodeStatus::RUNNING;
}

BT::NodeStatus PerformRoleDuty::tick() {
    auto* ctx = get_ctx(this);
    if (!ctx || !ctx->registry || !ctx->world) return BT::NodeStatus::FAILURE;
    if (!ctx->registry->valid(ctx->entity)) return BT::NodeStatus::FAILURE;

    auto* pos  = ctx->registry->try_get<PositionComponent>(ctx->entity);
    auto* role = ctx->registry->try_get<SocialRoleComponent>(ctx->entity);
    if (!pos || !role) return BT::NodeStatus::FAILURE;

    auto& region = ctx->world->region(pos->region_id);

    switch (role->role) {
        case SocialRole::GATHERER:
        case SocialRole::FARMER: {
            // Produce food
            uint8_t grain = static_cast<uint8_t>(ResourceType::FOOD_GRAIN);
            region.resource_amount[grain] = std::min(
                region.resource_amount[grain] + 0.5f, region.resource_cap[grain]);
            return BT::NodeStatus::SUCCESS;
        }
        case SocialRole::MINER: {
            uint8_t iron = static_cast<uint8_t>(ResourceType::IRON_ORE);
            region.resource_amount[iron] = std::min(
                region.resource_amount[iron] + 0.3f, region.resource_cap[iron]);
            return BT::NodeStatus::SUCCESS;
        }
        case SocialRole::CRAFTSMAN: {
            uint8_t tools = static_cast<uint8_t>(ResourceType::TOOLS);
            uint8_t iron  = static_cast<uint8_t>(ResourceType::IRON_ORE);
            if (region.resource_amount[iron] > 1.0f) {
                region.resource_amount[iron]  -= 1.0f;
                region.resource_amount[tools] = std::min(
                    region.resource_amount[tools] + 0.5f, region.resource_cap[tools]);
            }
            return BT::NodeStatus::SUCCESS;
        }
        case SocialRole::SOLDIER:
            // Increase region's defense passively
            region.env.sickness_risk = std::max(0.0f, region.env.sickness_risk - 0.0001f);
            return BT::NodeStatus::SUCCESS;
        default:
            return BT::NodeStatus::SUCCESS;
    }
}

BT::NodeStatus Sleep::tick() {
    auto* ctx = get_ctx(this);
    if (!ctx || !ctx->registry) return BT::NodeStatus::FAILURE;
    if (!ctx->registry->valid(ctx->entity)) return BT::NodeStatus::FAILURE;

    float regen = 0.01f;
    getInput("regen_rate", regen);

    auto* needs = ctx->registry->try_get<NeedsComponent>(ctx->entity);
    if (!needs) return BT::NodeStatus::FAILURE;

    needs->delta(NeedType::REST, regen);
    needs->delta(NeedType::SOCIAL, 0.001f);  // slight social benefit from resting near others
    return BT::NodeStatus::SUCCESS;
}

BT::NodeStatus IdleWander::tick() {
    auto* ctx = get_ctx(this);
    if (!ctx || !ctx->registry || !ctx->world) return BT::NodeStatus::FAILURE;
    if (!ctx->registry->valid(ctx->entity)) return BT::NodeStatus::FAILURE;

    auto* pos = ctx->registry->try_get<PositionComponent>(ctx->entity);
    if (!pos) return BT::NodeStatus::FAILURE;

    // Deterministic wander: tick + entity hash → direction
    uint64_t h = ctx->current_tick * 2654435761ULL ^
                 (static_cast<uint64_t>(pos->x) * 40503ULL) ^
                 (static_cast<uint64_t>(pos->y) * 6700417ULL);

    static const int32_t dx[] = {-1, 1, 0, 0, 0};
    static const int32_t dy[] = {0, 0, -1, 1, 0};
    int d = static_cast<int>(h % 5);

    int32_t nx = pos->x + dx[d];
    int32_t ny = pos->y + dy[d];
    if (ctx->world->config().width  > nx && nx >= 0 &&
        ctx->world->config().height > ny && ny >= 0 &&
        ctx->world->cell(nx, ny).navigable) {
        pos->x = nx; pos->y = ny;
        pos->region_id = ctx->world->region_id_at(nx, ny);
    }
    return BT::NodeStatus::SUCCESS;
}

// ---------------------------------------------------------------------------
// SomniTreeFactory
// ---------------------------------------------------------------------------

void SomniTreeFactory::register_nodes(BT::BehaviorTreeFactory& factory) {
    factory.registerNodeType<IsHungry>("IsHungry");
    factory.registerNodeType<IsThirsty>("IsThirsty");
    factory.registerNodeType<IsUnsafe>("IsUnsafe");
    factory.registerNodeType<IsTired>("IsTired");
    factory.registerNodeType<HasResourceInInventory>("HasResourceInInventory");
    factory.registerNodeType<EnemyNearby>("EnemyNearby");

    factory.registerNodeType<MoveToTarget>("MoveToTarget");
    factory.registerNodeType<GatherResource>("GatherResource");
    factory.registerNodeType<ConsumeFood>("ConsumeFood");
    factory.registerNodeType<ConsumeWater>("ConsumeWater");
    factory.registerNodeType<RecallResourceLocation>("RecallResourceLocation");
    factory.registerNodeType<FleeFromThreat>("FleeFromThreat");
    factory.registerNodeType<AttackEnemy>("AttackEnemy");
    factory.registerNodeType<PerformRoleDuty>("PerformRoleDuty");
    factory.registerNodeType<Sleep>("Sleep");
    factory.registerNodeType<IdleWander>("IdleWander");
}

BT::BehaviorTreeFactory SomniTreeFactory::build() {
    BT::BehaviorTreeFactory factory;
    register_nodes(factory);
    factory.registerBehaviorTreeFromText(default_npc_tree_xml());
    return factory;
}

std::string SomniTreeFactory::default_npc_tree_xml() {
    return R"(
<root BTCPP_format="4">
  <BehaviorTree ID="NPCDefault">
    <Selector>

      <!-- SAFETY: flee or fight if threatened -->
      <Sequence>
        <IsUnsafe threshold="0.30"/>
        <Selector>
          <Sequence>
            <EnemyNearby radius="6.0"/>
            <AttackEnemy/>
          </Sequence>
          <FleeFromThreat/>
        </Selector>
      </Sequence>

      <!-- THIRST: drink water -->
      <Sequence>
        <IsThirsty threshold="0.15"/>
        <Selector>
          <ConsumeWater/>
          <Sequence>
            <RecallResourceLocation resource_type="3" out_x="{recall_x}" out_y="{recall_y}"/>
            <MoveToTarget target_x="{recall_x}" target_y="{recall_y}"/>
            <GatherResource resource_type="3" amount="2.0"/>
            <ConsumeWater/>
          </Sequence>
        </Selector>
      </Sequence>

      <!-- HUNGER: eat food -->
      <Sequence>
        <IsHungry threshold="0.25"/>
        <Selector>
          <ConsumeFood/>
          <Sequence>
            <RecallResourceLocation resource_type="0" out_x="{recall_x}" out_y="{recall_y}"/>
            <MoveToTarget target_x="{recall_x}" target_y="{recall_y}"/>
            <GatherResource resource_type="0" amount="2.0"/>
            <ConsumeFood/>
          </Sequence>
          <Sequence>
            <GatherResource resource_type="0" amount="2.0"/>
            <ConsumeFood/>
          </Sequence>
        </Selector>
      </Sequence>

      <!-- REST: sleep if tired or night -->
      <Sequence>
        <IsTired threshold="0.15"/>
        <Sleep regen_rate="0.02"/>
      </Sequence>

      <!-- WORK: perform social role duty -->
      <PerformRoleDuty/>

      <!-- DEFAULT: idle wander -->
      <IdleWander/>

    </Selector>
  </BehaviorTree>
</root>
)";
}

}  // namespace somni

#pragma once
#include <somni/agents/Needs.hpp>
#include <somni/agents/Memory.hpp>
#include <somni/agents/StateMachine.hpp>
#include <somni/world/Resources.hpp>
#include <behaviortree_cpp/bt_factory.h>
#include <behaviortree_cpp/blackboard.h>
#include <entt/entt.hpp>
#include <cstdint>
#include <string>

namespace somni {

class WorldState;
class WorldMap;
class FactionRegistry;

// ---------------------------------------------------------------------------
// NPC execution context — passed via BT blackboard pointers
// All data is numeric; NO strings or language in runtime path
// ---------------------------------------------------------------------------
struct NPCBlackboard {
    // Identity (read-only during tick)
    entt::entity entity{entt::null};
    uint32_t     faction_id{0xFFFFFFFF};
    uint8_t      social_role{0};

    // Needs (updated each tick from NeedsComponent)
    float hunger{1.0f};
    float thirst{1.0f};
    float safety{1.0f};
    float rest{1.0f};
    float social{1.0f};

    // Navigation target
    int32_t target_x{0};
    int32_t target_y{0};
    bool    has_target{false};

    // Combat
    entt::entity enemy_entity{entt::null};
    bool         enemy_in_range{false};
    float        enemy_strength{0.0f};

    // Environment
    bool     is_night{false};
    uint64_t current_tick{0};
    uint64_t world_seed{0};

    // Shared world access (non-owning pointers, valid for lifetime of tick)
    WorldState*      world{nullptr};
    WorldMap*        map{nullptr};
    FactionRegistry* factions{nullptr};
    entt::registry*  registry{nullptr};
};

// ---------------------------------------------------------------------------
// Condition nodes — pure read, return SUCCESS or FAILURE
// ---------------------------------------------------------------------------

class IsHungry : public BT::ConditionNode {
public:
    IsHungry(const std::string& name, const BT::NodeConfig& cfg)
        : BT::ConditionNode(name, cfg) {}
    BT::NodeStatus tick() override;
    static BT::PortsList providedPorts() {
        return {BT::InputPort<float>("threshold", 0.25f, "hunger below this = hungry")};
    }
};

class IsThirsty : public BT::ConditionNode {
public:
    IsThirsty(const std::string& name, const BT::NodeConfig& cfg)
        : BT::ConditionNode(name, cfg) {}
    BT::NodeStatus tick() override;
    static BT::PortsList providedPorts() {
        return {BT::InputPort<float>("threshold", 0.15f, "thirst below this = thirsty")};
    }
};

class IsUnsafe : public BT::ConditionNode {
public:
    IsUnsafe(const std::string& name, const BT::NodeConfig& cfg)
        : BT::ConditionNode(name, cfg) {}
    BT::NodeStatus tick() override;
    static BT::PortsList providedPorts() {
        return {BT::InputPort<float>("threshold", 0.30f, "safety below this = unsafe")};
    }
};

class IsTired : public BT::ConditionNode {
public:
    IsTired(const std::string& name, const BT::NodeConfig& cfg)
        : BT::ConditionNode(name, cfg) {}
    BT::NodeStatus tick() override;
    static BT::PortsList providedPorts() {
        return {BT::InputPort<float>("threshold", 0.15f, "rest below this = tired")};
    }
};

class HasResourceInInventory : public BT::ConditionNode {
public:
    HasResourceInInventory(const std::string& name, const BT::NodeConfig& cfg)
        : BT::ConditionNode(name, cfg) {}
    BT::NodeStatus tick() override;
    static BT::PortsList providedPorts() {
        return {
            BT::InputPort<int>("resource_type", 0, "ResourceType enum value"),
            BT::InputPort<float>("min_amount", 0.01f, "minimum required"),
        };
    }
};

class EnemyNearby : public BT::ConditionNode {
public:
    EnemyNearby(const std::string& name, const BT::NodeConfig& cfg)
        : BT::ConditionNode(name, cfg) {}
    BT::NodeStatus tick() override;
    static BT::PortsList providedPorts() {
        return {BT::InputPort<float>("radius", 8.0f, "detection radius in cells")};
    }
};

// ---------------------------------------------------------------------------
// Action nodes — perform simulation mutations, return RUNNING or SUCCESS/FAILURE
// ---------------------------------------------------------------------------

class MoveToTarget : public BT::StatefulActionNode {
public:
    MoveToTarget(const std::string& name, const BT::NodeConfig& cfg)
        : BT::StatefulActionNode(name, cfg) {}
    BT::NodeStatus onStart()    override;
    BT::NodeStatus onRunning()  override;
    void           onHalted()   override {}
    static BT::PortsList providedPorts() {
        return {
            BT::InputPort<int>("target_x"),
            BT::InputPort<int>("target_y"),
        };
    }
};

class GatherResource : public BT::SyncActionNode {
public:
    GatherResource(const std::string& name, const BT::NodeConfig& cfg)
        : BT::SyncActionNode(name, cfg) {}
    BT::NodeStatus tick() override;
    static BT::PortsList providedPorts() {
        return {
            BT::InputPort<int>("resource_type"),
            BT::InputPort<float>("amount", 1.0f),
        };
    }
};

class ConsumeFood : public BT::SyncActionNode {
public:
    ConsumeFood(const std::string& name, const BT::NodeConfig& cfg)
        : BT::SyncActionNode(name, cfg) {}
    BT::NodeStatus tick() override;
    static BT::PortsList providedPorts() { return {}; }
};

class ConsumeWater : public BT::SyncActionNode {
public:
    ConsumeWater(const std::string& name, const BT::NodeConfig& cfg)
        : BT::SyncActionNode(name, cfg) {}
    BT::NodeStatus tick() override;
    static BT::PortsList providedPorts() { return {}; }
};

class RecallResourceLocation : public BT::SyncActionNode {
public:
    RecallResourceLocation(const std::string& name, const BT::NodeConfig& cfg)
        : BT::SyncActionNode(name, cfg) {}
    BT::NodeStatus tick() override;
    static BT::PortsList providedPorts() {
        return {
            BT::InputPort<int>("resource_type"),
            BT::OutputPort<int>("out_x"),
            BT::OutputPort<int>("out_y"),
        };
    }
};

class FleeFromThreat : public BT::StatefulActionNode {
public:
    FleeFromThreat(const std::string& name, const BT::NodeConfig& cfg)
        : BT::StatefulActionNode(name, cfg) {}
    BT::NodeStatus onStart()   override;
    BT::NodeStatus onRunning() override;
    void           onHalted()  override {}
    static BT::PortsList providedPorts() { return {}; }
};

class AttackEnemy : public BT::SyncActionNode {
public:
    AttackEnemy(const std::string& name, const BT::NodeConfig& cfg)
        : BT::SyncActionNode(name, cfg) {}
    BT::NodeStatus tick() override;
    static BT::PortsList providedPorts() { return {}; }
};

class PerformRoleDuty : public BT::SyncActionNode {
public:
    PerformRoleDuty(const std::string& name, const BT::NodeConfig& cfg)
        : BT::SyncActionNode(name, cfg) {}
    BT::NodeStatus tick() override;
    static BT::PortsList providedPorts() { return {}; }
};

class Sleep : public BT::SyncActionNode {
public:
    Sleep(const std::string& name, const BT::NodeConfig& cfg)
        : BT::SyncActionNode(name, cfg) {}
    BT::NodeStatus tick() override;
    static BT::PortsList providedPorts() {
        return {BT::InputPort<float>("regen_rate", 0.01f, "rest restored per tick")};
    }
};

class IdleWander : public BT::SyncActionNode {
public:
    IdleWander(const std::string& name, const BT::NodeConfig& cfg)
        : BT::SyncActionNode(name, cfg) {}
    BT::NodeStatus tick() override;
    static BT::PortsList providedPorts() { return {}; }
};

// ---------------------------------------------------------------------------
// BT factory builder — registers all SOMNI nodes with BT::BehaviorTreeFactory
// Call once at startup; reuse factory for all NPCs
// ---------------------------------------------------------------------------
class SomniTreeFactory {
public:
    static BT::BehaviorTreeFactory build();

    // Build the default NPC behavior tree XML
    static std::string default_npc_tree_xml();
    // Build a variant tree for a given social role
    static std::string role_tree_xml(uint8_t social_role);

private:
    static void register_nodes(BT::BehaviorTreeFactory& factory);
};

// ---------------------------------------------------------------------------
// Per-NPC behavior tree instance
// ---------------------------------------------------------------------------
struct BehaviorTreeComponent {
    BT::Tree             tree;
    BT::Blackboard::Ptr  blackboard;
    NPCBlackboard*       ctx{nullptr};  // points into component storage, not heap
    bool                 initialized{false};

    void sync_from_blackboard(NPCBlackboard& src) {
        ctx = &src;
        blackboard->set("_ctx", ctx);
    }
};

}  // namespace somni

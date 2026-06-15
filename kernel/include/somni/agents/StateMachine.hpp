#pragma once
#include <array>
#include <cstdint>
#include <functional>

namespace somni {

// ---------------------------------------------------------------------------
// High-level NPC state — drives which behavior tree subtree is active
// Transitions are deterministic: triggered by threshold conditions
// ---------------------------------------------------------------------------
enum class NPCState : uint8_t {
    IDLE      = 0,   // no pressing need, wandering
    FORAGING  = 1,   // seeking food or water
    WORKING   = 2,   // performing assigned social role
    FLEEING   = 3,   // moving away from threats
    FIGHTING  = 4,   // engaged in combat
    SLEEPING  = 5,   // resting to restore stamina/rest need
    TRADING   = 6,   // exchanging resources with another NPC
    SOCIALIZING = 7, // interacting with allies
    MIGRATING = 8,   // moving to a new region
    DEAD      = 9,
    COUNT     = 10
};

constexpr uint8_t NPC_STATE_COUNT = static_cast<uint8_t>(NPCState::COUNT);

// ---------------------------------------------------------------------------
// Transition event triggers
// ---------------------------------------------------------------------------
enum class StateTransitionTrigger : uint8_t {
    HUNGER_CRITICAL  = 0,
    THIRST_CRITICAL  = 1,
    THREAT_DETECTED  = 2,
    THREAT_CLEARED   = 3,
    REST_CRITICAL    = 4,
    TASK_COMPLETE    = 5,
    TRADE_OFFER      = 6,
    FIGHT_WON        = 7,
    FIGHT_LOST       = 8,
    DIED             = 9,
    NEED_MET         = 10,
    COUNT            = 11
};

// ---------------------------------------------------------------------------
// FSM component — tracks state history for 4 past states
// ---------------------------------------------------------------------------
struct StateMachineComponent {
    NPCState current{NPCState::IDLE};
    NPCState previous{NPCState::IDLE};
    uint64_t entered_tick{0};
    uint32_t state_duration_ticks{0};

    // Ring buffer of last 4 states for pattern detection
    std::array<NPCState, 4> history{};
    uint8_t                 history_head{0};

    void transition_to(NPCState next, uint64_t tick) {
        history[history_head] = current;
        history_head = (history_head + 1) % 4;
        previous     = current;
        current      = next;
        entered_tick = tick;
        state_duration_ticks = 0;
    }

    bool in_state(NPCState s) const { return current == s; }
    bool just_entered()        const { return state_duration_ticks == 0; }
};

// ---------------------------------------------------------------------------
// Deterministic FSM transition table
// Conditions evaluated in priority order — first match wins
// ---------------------------------------------------------------------------
struct StateTransition {
    NPCState from{NPCState::IDLE};
    NPCState to{NPCState::IDLE};
    StateTransitionTrigger trigger{StateTransitionTrigger::COUNT};
    float    priority{0.0f};  // higher = evaluated first
};

class StateMachine {
public:
    // Evaluate all possible transitions from current state given NPC context
    // Returns true if a transition occurred
    template <typename NPCContext>
    static bool evaluate(StateMachineComponent& fsm, const NPCContext& ctx, uint64_t tick) {
        NPCState next = fsm.current;

        // Priority order: safety > survival > rest > work > social > idle
        if (ctx.safety < 0.25f && fsm.current != NPCState::FLEEING &&
            fsm.current != NPCState::FIGHTING && fsm.current != NPCState::DEAD) {
            next = ctx.enemy_in_range ? NPCState::FIGHTING : NPCState::FLEEING;
        } else if (ctx.hunger < 0.15f || ctx.thirst < 0.10f) {
            next = NPCState::FORAGING;
        } else if (ctx.rest < 0.10f && !ctx.is_night) {
            // wait for night to sleep unless critically tired
            if (ctx.rest < 0.05f) next = NPCState::SLEEPING;
        } else if (ctx.is_night && ctx.rest < 0.5f) {
            next = NPCState::SLEEPING;
        } else if (fsm.current == NPCState::SLEEPING && ctx.rest > 0.9f) {
            next = NPCState::IDLE;
        } else if (fsm.current == NPCState::FLEEING && ctx.safety > 0.7f) {
            next = NPCState::IDLE;
        } else if (fsm.current == NPCState::FORAGING &&
                   ctx.hunger > 0.6f && ctx.thirst > 0.6f) {
            next = NPCState::WORKING;
        } else if (fsm.current == NPCState::IDLE) {
            next = NPCState::WORKING;
        }

        if (next != fsm.current) {
            fsm.transition_to(next, tick);
            return true;
        }
        ++fsm.state_duration_ticks;
        return false;
    }
};

}  // namespace somni

#pragma once
#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace somni {

// ---------------------------------------------------------------------------
// Base event — all domain events inherit from this
// ---------------------------------------------------------------------------
// Tag base. Intentionally trivial / no virtual members: events are always
// dispatched by their exact static type (type_index + static_cast), never
// stored or deleted polymorphically. Keeping this an empty aggregate base
// lets every derived event use brace-initialization, e.g. TickEvent{42, 0.0}.
struct Event {};

// ---------------------------------------------------------------------------
// Type-safe, thread-safe event bus
// ---------------------------------------------------------------------------
class EventBus {
public:
    using HandlerID = uint64_t;

    template <typename E>
    HandlerID subscribe(std::function<void(const E&)> handler) {
        static_assert(std::is_base_of<Event, E>::value, "E must derive from Event");
        std::lock_guard<std::mutex> lk(mtx_);
        auto id = next_id_++;
        handlers_[std::type_index(typeid(E))].push_back(
            {id, [h = std::move(handler)](const Event& e) { h(static_cast<const E&>(e)); }});
        return id;
    }

    template <typename E>
    void emit(const E& event) {
        static_assert(std::is_base_of<Event, E>::value, "E must derive from Event");
        std::lock_guard<std::mutex> lk(mtx_);
        auto it = handlers_.find(std::type_index(typeid(E)));
        if (it == handlers_.end()) return;
        for (auto& [id, fn] : it->second) fn(event);
    }

    void unsubscribe(HandlerID id) {
        std::lock_guard<std::mutex> lk(mtx_);
        for (auto& [type, vec] : handlers_) {
            vec.erase(std::remove_if(vec.begin(), vec.end(),
                          [id](const auto& p) { return p.first == id; }),
                      vec.end());
        }
    }

    void clear() {
        std::lock_guard<std::mutex> lk(mtx_);
        handlers_.clear();
    }

private:
    using Handler = std::pair<HandlerID, std::function<void(const Event&)>>;
    std::unordered_map<std::type_index, std::vector<Handler>> handlers_;
    HandlerID next_id_{0};
    std::mutex mtx_;
};

// ---------------------------------------------------------------------------
// Domain events
// ---------------------------------------------------------------------------

struct TickEvent : Event {
    uint64_t tick;
    double   simulation_time;  // in-world seconds
};

struct NPCDiedEvent : Event {
    uint32_t npc_id;
    enum Cause : uint8_t { STARVATION = 0, DEHYDRATION, COMBAT, ILLNESS, OLD_AGE } cause;
    uint32_t region_id;
};

struct NPCBornEvent : Event {
    uint32_t npc_id;
    uint32_t parent_a;
    uint32_t parent_b;
    uint32_t faction_id;
    uint32_t region_id;
};

struct FactionWarStartEvent : Event {
    uint32_t attacker_id;
    uint32_t defender_id;
    uint32_t trigger_region;
};

struct FactionPeaceEvent : Event {
    uint32_t faction_a;
    uint32_t faction_b;
};

struct ResourceDepletedEvent : Event {
    uint32_t region_id;
    uint8_t  resource_type;
};

struct ResourceDiscoveredEvent : Event {
    uint32_t npc_id;
    uint32_t region_id;
    uint8_t  resource_type;
    float    amount;
};

struct TradeCompletedEvent : Event {
    uint32_t faction_a;
    uint32_t faction_b;
    uint8_t  resource_type;
    float    amount;
    float    price;
};

struct BattleResolvedEvent : Event {
    uint32_t attacker_faction;
    uint32_t defender_faction;
    uint32_t region_id;
    uint32_t attacker_losses;
    uint32_t defender_losses;
    bool     attacker_won;
};

struct PlayerCommandEvent : Event {
    enum CommandType : uint8_t {
        SPAWN_NPC,
        KILL_NPC,
        SET_FACTION_RELATION,
        ADD_RESOURCE,
        REMOVE_RESOURCE,
        TRIGGER_DISASTER,
        ADVANCE_TICKS
    } type;
    uint64_t param_a;
    uint64_t param_b;
    float    param_f;
};

struct WorldSnapshotRequestEvent : Event {
    std::string output_path;
    uint64_t    tick;
};

}  // namespace somni

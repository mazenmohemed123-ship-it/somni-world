#pragma once
#include <array>
#include <cstdint>
#include <limits>
#include <optional>

namespace somni {

// ---------------------------------------------------------------------------
// Non-linguistic memory — purely numeric spatial + resource knowledge
// ---------------------------------------------------------------------------

enum class MemoryTag : uint8_t {
    FOOD_SOURCE  = 0,
    WATER_SOURCE = 1,
    SHELTER      = 2,
    THREAT       = 3,
    ALLY         = 4,
    TRADE_POST   = 5,
    UNKNOWN      = 0xFF
};

struct MemoryEntry {
    int32_t    world_x{0};
    int32_t    world_y{0};
    MemoryTag  tag{MemoryTag::UNKNOWN};
    float      value{0.0f};         // resource amount or threat strength
    float      confidence{0.0f};    // [0,1] decays over time
    uint64_t   tick_recorded{0};
    uint32_t   associated_id{0};    // faction_id, npc_id, etc.

    bool valid() const { return tag != MemoryTag::UNKNOWN && confidence > 0.01f; }
};

// ---------------------------------------------------------------------------
// Fixed-capacity memory ring buffer — no dynamic allocation
// 32 entries per NPC: enough for meaningful recall, cheap to update
// ---------------------------------------------------------------------------
struct MemoryComponent {
    static constexpr uint8_t CAPACITY = 32;

    std::array<MemoryEntry, CAPACITY> entries{};
    uint8_t head{0};   // next write position
    uint8_t size{0};   // number of valid entries

    // Write a new memory entry (evicts oldest if full)
    void remember(MemoryEntry e) {
        entries[head] = e;
        head = static_cast<uint8_t>((head + 1) % CAPACITY);
        if (size < CAPACITY) ++size;
    }

    // Decay confidence of all entries by delta per tick
    void decay(float delta_per_tick) {
        for (uint8_t i = 0; i < size; ++i) {
            entries[i].confidence -= delta_per_tick;
            if (entries[i].confidence < 0.0f) entries[i].confidence = 0.0f;
        }
    }

    // Find the best-known location for a given tag (highest confidence)
    std::optional<MemoryEntry> recall(MemoryTag tag) const {
        const MemoryEntry* best = nullptr;
        for (uint8_t i = 0; i < size; ++i) {
            const auto& e = entries[i];
            if (e.tag == tag && e.valid()) {
                if (!best || e.confidence > best->confidence) best = &e;
            }
        }
        return best ? std::optional<MemoryEntry>(*best) : std::nullopt;
    }

    // Update confidence for a known location
    void reinforce(int32_t x, int32_t y, MemoryTag tag, float new_value, uint64_t tick) {
        for (uint8_t i = 0; i < size; ++i) {
            auto& e = entries[i];
            if (e.tag == tag && e.world_x == x && e.world_y == y) {
                e.confidence   = std::min(1.0f, e.confidence + 0.3f);
                e.value        = new_value;
                e.tick_recorded = tick;
                return;
            }
        }
        // Not found — add new
        remember({x, y, tag, new_value, 0.9f, tick, 0});
    }

    // Forget all threats (called after reaching safety)
    void forget_threats() {
        for (uint8_t i = 0; i < size; ++i) {
            if (entries[i].tag == MemoryTag::THREAT) entries[i].confidence = 0.0f;
        }
    }
};

}  // namespace somni

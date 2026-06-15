#pragma once
#include <array>
#include <cstdint>
#include <algorithm>

namespace somni {

// ---------------------------------------------------------------------------
// NPC need indices — purely numeric, no language
// ---------------------------------------------------------------------------
enum class NeedType : uint8_t {
    HUNGER  = 0,
    THIRST  = 1,
    SAFETY  = 2,
    REST    = 3,
    SOCIAL  = 4,
    COUNT   = 5
};

constexpr uint8_t NEED_COUNT = static_cast<uint8_t>(NeedType::COUNT);

// ---------------------------------------------------------------------------
// ECS component: current need values [0.0 = critical, 1.0 = fully satisfied]
// ---------------------------------------------------------------------------
struct NeedsComponent {
    std::array<float, NEED_COUNT> values;

    NeedsComponent() { values.fill(1.0f); }

    float get(NeedType n) const { return values[static_cast<uint8_t>(n)]; }
    void  set(NeedType n, float v) {
        values[static_cast<uint8_t>(n)] = std::clamp(v, 0.0f, 1.0f);
    }
    void  delta(NeedType n, float d) { set(n, get(n) + d); }

    // Composite urgency — higher = more desperate [0, 1]
    float urgency(NeedType n) const { return 1.0f - get(n); }
    float max_urgency() const {
        float mx = 0.0f;
        for (auto v : values) mx = std::max(mx, 1.0f - v);
        return mx;
    }
    bool  critical(NeedType n) const { return get(n) < critical_threshold(n); }

    static constexpr float critical_threshold(NeedType n) {
        switch (n) {
            case NeedType::HUNGER:  return 0.15f;
            case NeedType::THIRST:  return 0.10f;
            case NeedType::SAFETY:  return 0.25f;
            case NeedType::REST:    return 0.10f;
            case NeedType::SOCIAL:  return 0.05f;
            default:                return 0.10f;
        }
    }
};

// ---------------------------------------------------------------------------
// ECS component: how fast each need decays per simulation tick
// Values are negative deltas per tick (units / tick)
// ---------------------------------------------------------------------------
struct NeedsDecayComponent {
    std::array<float, NEED_COUNT> rates;

    // Default decay rates for a baseline human-like NPC
    NeedsDecayComponent() {
        rates[static_cast<uint8_t>(NeedType::HUNGER)] = 0.0002f;  // starves in ~5000 ticks
        rates[static_cast<uint8_t>(NeedType::THIRST)] = 0.0004f;  // dehydrates in ~2500 ticks
        rates[static_cast<uint8_t>(NeedType::SAFETY)] = 0.0f;     // modified by environment
        rates[static_cast<uint8_t>(NeedType::REST)]   = 0.0001f;  // exhausted in ~10000 ticks
        rates[static_cast<uint8_t>(NeedType::SOCIAL)] = 0.00005f; // slow social decay
    }
};

// ---------------------------------------------------------------------------
// Utility AI: converts needs into action utility scores
// Higher score = higher priority to satisfy
// ---------------------------------------------------------------------------
struct NeedsUtility {
    // Exponential urgency curve — small deficits score low, critical deficits score very high
    static float score(float need_value, float weight = 1.0f) {
        float deficit = 1.0f - std::clamp(need_value, 0.0f, 1.0f);
        return weight * deficit * deficit;  // quadratic urgency
    }

    static float hunger_score(const NeedsComponent& n) {
        return score(n.get(NeedType::HUNGER), 1.2f);
    }
    static float thirst_score(const NeedsComponent& n) {
        return score(n.get(NeedType::THIRST), 1.5f);  // thirst is more urgent
    }
    static float safety_score(const NeedsComponent& n) {
        return score(n.get(NeedType::SAFETY), 2.0f);  // safety overrides everything
    }
    static float rest_score(const NeedsComponent& n) {
        return score(n.get(NeedType::REST), 0.8f);
    }
    static float social_score(const NeedsComponent& n) {
        return score(n.get(NeedType::SOCIAL), 0.3f);
    }
};

}  // namespace somni

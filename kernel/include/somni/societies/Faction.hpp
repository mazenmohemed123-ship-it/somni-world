#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace somni {

using FactionID = uint32_t;
constexpr FactionID FACTION_NONE = 0xFFFFFFFF;
constexpr uint32_t  MAX_FACTIONS = 64;

// ---------------------------------------------------------------------------
// Faction ideology — determines decision-making tendencies
// ---------------------------------------------------------------------------
enum class FactionIdeology : uint8_t {
    TRIBAL      = 0,  // survival-first, small groups
    AGRARIAN    = 1,  // expansion through farming
    MILITARIST  = 2,  // power through conquest
    MERCANTILE  = 3,  // wealth through trade
    THEOCRATIC  = 4,  // control through religion/ideology
    NOMADIC     = 5,  // mobility-based economy
};

// ---------------------------------------------------------------------------
// Government type — affects internal rule propagation
// ---------------------------------------------------------------------------
enum class GovernmentType : uint8_t {
    CHIEFTAINCY  = 0,
    MONARCHY     = 1,
    OLIGARCHY    = 2,
    REPUBLIC     = 3,
    THEOCRACY    = 4,
};

// ---------------------------------------------------------------------------
// Faction data (POD-friendly, serializable)
// ---------------------------------------------------------------------------
struct FactionData {
    FactionID       id{FACTION_NONE};
    uint64_t        name_hash{0};           // hash of name string; string stored externally
    FactionIdeology ideology{FactionIdeology::TRIBAL};
    GovernmentType  government{GovernmentType::CHIEFTAINCY};

    // Military
    uint32_t military_strength{0};          // total combat units
    uint32_t military_cap{0};              // max recruiteable units
    float    military_morale{0.75f};        // [0,1] — affects battle outcome

    // Economy
    float    treasury{100.0f};
    float    income_per_tick{0.0f};
    float    expenditure_per_tick{0.0f};

    // Population
    uint32_t population{0};
    float    population_growth_rate{0.0001f};  // per tick

    // Territory: set of region IDs owned
    std::unordered_set<uint32_t> territory_regions;

    // Resources stockpiled [indexed by ResourceType enum]
    std::array<float, 16> stockpile{};

    // Internal stability [0,1] — low = risk of rebellion
    float stability{0.8f};

    bool is_valid() const { return id != FACTION_NONE; }
};

// ---------------------------------------------------------------------------
// Diplomatic relation between two factions [-1.0 hostile, +1.0 allied]
// ---------------------------------------------------------------------------
struct DiplomaticRelation {
    float    value{0.0f};   // [-1, 1]
    bool     at_war{false};
    bool     has_treaty{false};
    uint64_t treaty_expires_tick{0};

    bool is_hostile()  const { return value < -0.3f || at_war; }
    bool is_neutral()  const { return !is_hostile() && value < 0.3f; }
    bool is_friendly() const { return value >= 0.3f && !at_war; }
};

// ---------------------------------------------------------------------------
// Dense upper-triangular diplomacy matrix for N factions
// ---------------------------------------------------------------------------
class DiplomacyMatrix {
public:
    DiplomacyMatrix() = default;

    DiplomaticRelation& get(FactionID a, FactionID b) {
        auto [lo, hi] = sorted(a, b);
        return matrix_[lo][hi];
    }
    const DiplomaticRelation& get(FactionID a, FactionID b) const {
        auto [lo, hi] = sorted(a, b);
        return matrix_[lo][hi];
    }

    void set_relation(FactionID a, FactionID b, float val) {
        get(a, b).value = std::clamp(val, -1.0f, 1.0f);
    }

    void adjust_relation(FactionID a, FactionID b, float delta) {
        auto& r = get(a, b);
        r.value = std::clamp(r.value + delta, -1.0f, 1.0f);
    }

    void declare_war(FactionID a, FactionID b) {
        auto& r = get(a, b);
        r.at_war = true;
        r.value  = std::min(r.value, -0.5f);
    }

    void sign_peace(FactionID a, FactionID b) {
        auto& r = get(a, b);
        r.at_war = false;
        r.value  = std::max(r.value, -0.1f);
    }

private:
    static std::pair<FactionID, FactionID> sorted(FactionID a, FactionID b) {
        return a < b ? std::make_pair(a, b) : std::make_pair(b, a);
    }

    // Statically sized matrix — MAX_FACTIONS × MAX_FACTIONS
    DiplomaticRelation matrix_[MAX_FACTIONS][MAX_FACTIONS];
};

// ---------------------------------------------------------------------------
// Registry: owns all faction data + diplomacy
// ---------------------------------------------------------------------------
class FactionRegistry {
public:
    FactionID create_faction(FactionData data);
    void      destroy_faction(FactionID id);

    FactionData*       get(FactionID id);
    const FactionData* get(FactionID id) const;

    DiplomacyMatrix& diplomacy()             { return diplomacy_; }
    const DiplomacyMatrix& diplomacy() const { return diplomacy_; }

    std::vector<FactionID> all_ids() const;
    uint32_t count() const { return static_cast<uint32_t>(factions_.size()); }

    // Tick-level updates (growth, stability, income)
    void tick(uint64_t current_tick);

private:
    std::unordered_map<FactionID, FactionData> factions_;
    DiplomacyMatrix                            diplomacy_;
    FactionID                                  next_id_{1};
};

}  // namespace somni

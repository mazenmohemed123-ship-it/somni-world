#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace somni {

// ---------------------------------------------------------------------------
// Biome classification (deterministic from terrain values)
// ---------------------------------------------------------------------------
enum class Biome : uint8_t {
    OCEAN         = 0,
    TUNDRA        = 1,
    BOREAL_FOREST = 2,
    TEMPERATE_FOREST = 3,
    GRASSLAND     = 4,
    SAVANNA       = 5,
    DESERT        = 6,
    JUNGLE        = 7,
    SWAMP         = 8,
    MOUNTAINS     = 9,
    ICE_SHEET     = 10,
};

// ---------------------------------------------------------------------------
// Single terrain cell — smallest spatial unit
// ---------------------------------------------------------------------------
struct TerrainCell {
    float   elevation{0.0f};    // [0,1]: 0=ocean, 1=peak
    float   moisture{0.5f};     // [0,1]: 0=arid, 1=waterlogged
    float   temperature{0.5f};  // [0,1]: 0=polar, 1=equatorial
    float   fertility{0.0f};    // [0,1]: agricultural potential
    Biome   biome{Biome::OCEAN};
    bool    navigable{false};   // can NPCs traverse?
    bool    has_river{false};
};

// ---------------------------------------------------------------------------
// In-world calendar and clock
// ---------------------------------------------------------------------------
struct WorldClock {
    // tick is the single source of truth; all calendar fields are derived
    // from it in advance(). Start at tick = 6h so hour{6} is consistent.
    uint64_t tick{6 * 60};     // absolute simulation tick (offset = 6 * TICKS_PER_HOUR)
    uint32_t year{1};
    uint8_t  season{0};        // 0=spring, 1=summer, 2=autumn, 3=winter
    uint8_t  month{1};         // 1–12
    uint8_t  day{1};           // 1–30
    uint8_t  hour{6};          // 0–23

    static constexpr uint32_t TICKS_PER_HOUR   = 60;
    static constexpr uint32_t TICKS_PER_DAY    = TICKS_PER_HOUR * 24;
    static constexpr uint32_t TICKS_PER_MONTH  = TICKS_PER_DAY  * 30;
    static constexpr uint32_t TICKS_PER_SEASON = TICKS_PER_MONTH * 3;
    static constexpr uint32_t TICKS_PER_YEAR   = TICKS_PER_SEASON * 4;

    void advance(uint64_t n_ticks = 1);
    double to_simulation_seconds() const { return static_cast<double>(tick); }
    bool is_night() const { return hour < 6 || hour >= 20; }
    bool is_winter() const { return season == 3; }

    nlohmann::json to_json() const;
};

// ---------------------------------------------------------------------------
// World configuration — set once at bootstrap, then immutable
// ---------------------------------------------------------------------------
struct WorldConfig {
    uint64_t    seed{42};
    std::string name{"unnamed"};
    int32_t     width{512};          // cells
    int32_t     height{512};         // cells
    uint32_t    region_size{16};     // cells per region edge
    uint32_t    ticks_per_second{20};// simulation tick rate
    float       time_scale{1.0f};    // 1.0 = real time, 10.0 = 10× faster

    int32_t     region_grid_w() const { return width  / static_cast<int32_t>(region_size); }
    int32_t     region_grid_h() const { return height / static_cast<int32_t>(region_size); }
    uint32_t    total_regions()  const {
        return static_cast<uint32_t>(region_grid_w() * region_grid_h());
    }
    uint32_t    total_cells()    const {
        return static_cast<uint32_t>(width * height);
    }

    nlohmann::json to_json() const;
    static WorldConfig from_json(const nlohmann::json& j);
};

// ---------------------------------------------------------------------------
// Environmental modifiers — per-region seasonal effects
// ---------------------------------------------------------------------------
struct EnvironmentModifier {
    float food_yield_mult{1.0f};
    float water_availability{1.0f};
    float movement_cost{1.0f};     // higher = slower NPC movement
    float sickness_risk{0.0f};
};

// ---------------------------------------------------------------------------
// Region: aggregate state for a block of terrain cells
// ---------------------------------------------------------------------------
struct RegionState {
    uint32_t            id{0};
    int32_t             grid_x{0};
    int32_t             grid_y{0};
    Biome               dominant_biome{Biome::GRASSLAND};
    EnvironmentModifier env{};

    // Resource pools — 16 resource types, indexed by ResourceType enum
    std::array<float, 16> resource_amount{};
    std::array<float, 16> resource_regen{};    // per-tick regen
    std::array<float, 16> resource_cap{};      // maximum

    // Population counters (derived from ECS query, cached)
    uint32_t npc_count{0};
    uint32_t hostile_npc_count{0};

    // Ownership
    uint32_t controlling_faction{0xFFFFFFFF};  // FACTION_NONE
    float    control_strength{0.0f};           // [0,1]

    // Cell bounds in world space
    int32_t cell_x0() const { return grid_x * 16; }
    int32_t cell_y0() const { return grid_y * 16; }
};

// ---------------------------------------------------------------------------
// Complete simulation world state
// ---------------------------------------------------------------------------
class WorldState {
public:
    WorldState() = default;
    explicit WorldState(WorldConfig config);

    // Configuration — immutable after init
    const WorldConfig& config() const { return config_; }

    // Clock
    WorldClock&       clock()       { return clock_; }
    const WorldClock& clock() const { return clock_; }

    // Terrain grid (width × height cells, row-major)
    TerrainCell& cell(int32_t x, int32_t y) {
        return terrain_[y * config_.width + x];
    }
    const TerrainCell& cell(int32_t x, int32_t y) const {
        return terrain_[y * config_.width + x];
    }

    // Region grid
    RegionState& region(uint32_t id)             { return regions_[id]; }
    const RegionState& region(uint32_t id) const { return regions_[id]; }
    RegionState& region(int32_t gx, int32_t gy)  {
        return regions_[gy * config_.region_grid_w() + gx];
    }

    uint32_t region_id_at(int32_t cell_x, int32_t cell_y) const {
        int32_t gx = cell_x / static_cast<int32_t>(config_.region_size);
        int32_t gy = cell_y / static_cast<int32_t>(config_.region_size);
        return static_cast<uint32_t>(gy * config_.region_grid_w() + gx);
    }

    std::vector<RegionState>& regions()             { return regions_; }
    const std::vector<RegionState>& regions() const { return regions_; }
    std::vector<TerrainCell>& terrain()              { return terrain_; }

    // Tick resources and environment for all regions
    void tick_resources();
    void tick_environment();

    // Serialization
    nlohmann::json  to_json() const;
    static WorldState from_json(const nlohmann::json& j);
    void save(const std::string& path) const;
    static WorldState load(const std::string& path);

private:
    WorldConfig              config_;
    WorldClock               clock_;
    std::vector<TerrainCell> terrain_;    // config_.total_cells() elements
    std::vector<RegionState> regions_;    // config_.total_regions() elements
};

}  // namespace somni

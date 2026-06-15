#pragma once
#include <somni/core/WorldState.hpp>
#include <cstdint>
#include <vector>

namespace somni {

// ---------------------------------------------------------------------------
// Terrain generation parameters
// ---------------------------------------------------------------------------
struct TerrainGenConfig {
    float elevation_scale{0.018f};   // FastNoise frequency for elevation
    int   elevation_octaves{6};
    float moisture_scale{0.025f};
    int   moisture_octaves{4};
    float temperature_scale{0.012f};

    // Biome classification thresholds
    float ocean_level{0.38f};
    float mountain_level{0.75f};
    float snow_temperature{0.2f};
    float desert_moisture{0.25f};
    float jungle_moisture{0.7f};
};

// ---------------------------------------------------------------------------
// River segment for hydrology
// ---------------------------------------------------------------------------
struct RiverSegment {
    int32_t x0, y0, x1, y1;
    float   flow{1.0f};
};

// ---------------------------------------------------------------------------
// Procedural terrain generator — uses FastNoise2 for all noise
// Deterministic: same seed → same world
// ---------------------------------------------------------------------------
class TerrainGenerator {
public:
    TerrainGenerator(uint64_t seed, const TerrainGenConfig& cfg = {});

    // Fill a WorldState's terrain grid
    void generate(WorldState& world);

private:
    void   generate_elevation(WorldState& world);
    void   generate_moisture(WorldState& world);
    void   generate_temperature(WorldState& world);
    void   classify_biomes(WorldState& world);
    void   compute_fertility(WorldState& world);
    void   trace_rivers(WorldState& world);
    void   mark_navigable(WorldState& world);

    Biome  classify_cell(const TerrainCell& c) const;
    std::vector<RiverSegment> find_rivers(WorldState& world);

    uint64_t        seed_;
    TerrainGenConfig cfg_;
};

// ---------------------------------------------------------------------------
// Region classifier — assigns dominant biome and initial resources
// ---------------------------------------------------------------------------
class RegionClassifier {
public:
    static void classify(WorldState& world);

private:
    static Biome   dominant_biome(const WorldState& w, int32_t gx, int32_t gy);
    static void    assign_resources(RegionState& r, Biome biome, uint64_t seed);
};

}  // namespace somni

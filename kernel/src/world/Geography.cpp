#include <somni/world/Geography.hpp>
#include <somni/world/Resources.hpp>
#include <FastNoise/FastNoise.h>
#include <spdlog/spdlog.h>
#include <cmath>
#include <algorithm>
#include <random>

namespace somni {

// ---------------------------------------------------------------------------
// TerrainGenerator
// ---------------------------------------------------------------------------

TerrainGenerator::TerrainGenerator(uint64_t seed, const TerrainGenConfig& cfg)
    : seed_(seed), cfg_(cfg) {}

void TerrainGenerator::generate(WorldState& world) {
    spdlog::info("Generating terrain {}×{} seed={}", world.config().width,
                 world.config().height, seed_);
    generate_elevation(world);
    generate_moisture(world);
    generate_temperature(world);
    classify_biomes(world);
    compute_fertility(world);
    trace_rivers(world);
    mark_navigable(world);
    spdlog::info("Terrain generation complete");
}

void TerrainGenerator::generate_elevation(WorldState& world) {
    int32_t W = world.config().width;
    int32_t H = world.config().height;

    auto simplex = FastNoise::New<FastNoise::Simplex>();
    auto fractal = FastNoise::New<FastNoise::FractalFBm>();
    fractal->SetSource(simplex);
    fractal->SetOctaveCount(cfg_.elevation_octaves);
    fractal->SetGain(0.5f);
    fractal->SetLacunarity(2.0f);

    std::vector<float> noise(W * H);
    fractal->GenUniformGrid2D(noise.data(), 0, 0, W, H,
                              cfg_.elevation_scale, static_cast<int>(seed_));

    // Normalize to [0,1] and apply island gradient
    float cx = W * 0.5f, cy = H * 0.5f;
    float max_dist = std::sqrt(cx * cx + cy * cy);

    float mn = *std::min_element(noise.begin(), noise.end());
    float mx = *std::max_element(noise.begin(), noise.end());
    float range = mx - mn;

    for (int32_t y = 0; y < H; ++y) {
        for (int32_t x = 0; x < W; ++x) {
            float n = (noise[y * W + x] - mn) / (range + 1e-6f);
            // Island gradient: lower elevation near edges
            float dx = (x - cx) / max_dist;
            float dy = (y - cy) / max_dist;
            float dist = std::sqrt(dx * dx + dy * dy);
            float gradient = 1.0f - dist * 0.8f;
            world.cell(x, y).elevation = std::clamp(n * gradient, 0.0f, 1.0f);
        }
    }
}

void TerrainGenerator::generate_moisture(WorldState& world) {
    int32_t W = world.config().width;
    int32_t H = world.config().height;

    auto simplex = FastNoise::New<FastNoise::Simplex>();
    auto fractal = FastNoise::New<FastNoise::FractalFBm>();
    fractal->SetSource(simplex);
    fractal->SetOctaveCount(cfg_.moisture_octaves);

    std::vector<float> noise(W * H);
    fractal->GenUniformGrid2D(noise.data(), 0, 0, W, H,
                              cfg_.moisture_scale, static_cast<int>(seed_ ^ 0xDEADBEEF));

    float mn = *std::min_element(noise.begin(), noise.end());
    float mx = *std::max_element(noise.begin(), noise.end());
    float range = mx - mn;

    for (int32_t y = 0; y < H; ++y) {
        for (int32_t x = 0; x < W; ++x) {
            float n = (noise[y * W + x] - mn) / (range + 1e-6f);
            world.cell(x, y).moisture = n;
        }
    }
}

void TerrainGenerator::generate_temperature(WorldState& world) {
    int32_t W = world.config().width;
    int32_t H = world.config().height;

    auto simplex = FastNoise::New<FastNoise::Simplex>();
    std::vector<float> noise(W * H);
    simplex->GenUniformGrid2D(noise.data(), 0, 0, W, H,
                              cfg_.temperature_scale, static_cast<int>(seed_ ^ 0xCAFEBABE));

    float mn = *std::min_element(noise.begin(), noise.end());
    float mx = *std::max_element(noise.begin(), noise.end());
    float range = mx - mn;

    for (int32_t y = 0; y < H; ++y) {
        for (int32_t x = 0; x < W; ++x) {
            float n = (noise[y * W + x] - mn) / (range + 1e-6f);
            // Latitude gradient: cooler near top/bottom
            float lat = 1.0f - std::abs((float)y / H - 0.5f) * 2.0f;
            float temp = n * 0.3f + lat * 0.7f;
            // Elevation penalty: mountains are colder
            float elev = world.cell(x, y).elevation;
            temp -= elev * 0.4f;
            world.cell(x, y).temperature = std::clamp(temp, 0.0f, 1.0f);
        }
    }
}

Biome TerrainGenerator::classify_cell(const TerrainCell& c) const {
    if (c.elevation < cfg_.ocean_level)      return Biome::OCEAN;
    if (c.elevation > cfg_.mountain_level) {
        return c.temperature < cfg_.snow_temperature ? Biome::ICE_SHEET : Biome::MOUNTAINS;
    }
    if (c.temperature < cfg_.snow_temperature)       return Biome::TUNDRA;
    if (c.temperature < 0.35f) {
        return c.moisture > 0.5f ? Biome::BOREAL_FOREST : Biome::TUNDRA;
    }
    if (c.temperature < 0.65f) {
        if (c.moisture < cfg_.desert_moisture)        return Biome::GRASSLAND;
        if (c.moisture < 0.6f)                        return Biome::TEMPERATE_FOREST;
        return Biome::SWAMP;
    }
    // Hot
    if (c.moisture < cfg_.desert_moisture)            return Biome::DESERT;
    if (c.moisture < 0.5f)                            return Biome::SAVANNA;
    return Biome::JUNGLE;
}

void TerrainGenerator::classify_biomes(WorldState& world) {
    int32_t W = world.config().width;
    int32_t H = world.config().height;
    for (int32_t y = 0; y < H; ++y) {
        for (int32_t x = 0; x < W; ++x) {
            world.cell(x, y).biome = classify_cell(world.cell(x, y));
        }
    }
}

void TerrainGenerator::compute_fertility(WorldState& world) {
    int32_t W = world.config().width;
    int32_t H = world.config().height;
    for (int32_t y = 0; y < H; ++y) {
        for (int32_t x = 0; x < W; ++x) {
            auto& c = world.cell(x, y);
            float f = 0.0f;
            switch (c.biome) {
                case Biome::JUNGLE:           f = 0.9f; break;
                case Biome::TEMPERATE_FOREST: f = 0.7f; break;
                case Biome::GRASSLAND:        f = 0.8f; break;
                case Biome::SAVANNA:          f = 0.5f; break;
                case Biome::BOREAL_FOREST:    f = 0.4f; break;
                case Biome::SWAMP:            f = 0.6f; break;
                case Biome::TUNDRA:           f = 0.1f; break;
                case Biome::DESERT:           f = 0.05f; break;
                default:                      f = 0.0f; break;
            }
            c.fertility = f;
        }
    }
}

void TerrainGenerator::trace_rivers(WorldState& world) {
    // Simple river tracing: flow from high elevation toward ocean
    // Seeds determined by world seed for determinism
    std::mt19937_64 rng(seed_ ^ 0x123456789);
    int32_t W = world.config().width;
    int32_t H = world.config().height;
    uint32_t river_count = 8 + static_cast<uint32_t>(rng() % 8);

    for (uint32_t r = 0; r < river_count; ++r) {
        // Find a mountain starting point
        int32_t x = static_cast<int32_t>(rng() % W);
        int32_t y = static_cast<int32_t>(rng() % H);
        if (world.cell(x, y).elevation < 0.7f) continue;

        // Flow downhill
        for (int32_t step = 0; step < 500; ++step) {
            world.cell(x, y).has_river = true;
            world.cell(x, y).moisture  = std::min(1.0f, world.cell(x, y).moisture + 0.2f);

            // Find lowest neighbor
            int32_t best_x = x, best_y = y;
            float   best_elev = world.cell(x, y).elevation;
            int32_t dx[] = {-1, 1, 0, 0};
            int32_t dy[] = {0, 0, -1, 1};
            for (int d = 0; d < 4; ++d) {
                int32_t nx = x + dx[d], ny = y + dy[d];
                if (nx < 0 || nx >= W || ny < 0 || ny >= H) continue;
                if (world.cell(nx, ny).elevation < best_elev) {
                    best_elev = world.cell(nx, ny).elevation;
                    best_x = nx; best_y = ny;
                }
            }
            if (best_x == x && best_y == y) break;
            if (world.cell(best_x, best_y).biome == Biome::OCEAN) break;
            x = best_x; y = best_y;
        }
    }
}

void TerrainGenerator::mark_navigable(WorldState& world) {
    int32_t W = world.config().width;
    int32_t H = world.config().height;
    for (int32_t y = 0; y < H; ++y) {
        for (int32_t x = 0; x < W; ++x) {
            auto& c = world.cell(x, y);
            c.navigable = (c.biome != Biome::OCEAN && c.biome != Biome::ICE_SHEET &&
                           c.elevation < 0.9f);
        }
    }
}

// ---------------------------------------------------------------------------
// RegionClassifier
// ---------------------------------------------------------------------------

void RegionClassifier::classify(WorldState& world) {
    int32_t rw = world.config().region_grid_w();
    int32_t rh = world.config().region_grid_h();
    uint32_t rs = world.config().region_size;
    uint64_t seed = world.config().seed;

    for (int32_t gy = 0; gy < rh; ++gy) {
        for (int32_t gx = 0; gx < rw; ++gx) {
            uint32_t rid = static_cast<uint32_t>(gy * rw + gx);
            auto& r      = world.region(rid);
            r.dominant_biome = dominant_biome(world, gx, gy);
            assign_resources(r, r.dominant_biome, seed, rid);
        }
    }
}

Biome RegionClassifier::dominant_biome(const WorldState& w, int32_t gx, int32_t gy) {
    uint32_t rs = w.config().region_size;
    int32_t  cx = gx * rs + rs / 2;
    int32_t  cy = gy * rs + rs / 2;
    cx = std::clamp(cx, 0, w.config().width  - 1);
    cy = std::clamp(cy, 0, w.config().height - 1);
    return w.cell(cx, cy).biome;
}

void RegionClassifier::assign_resources(RegionState& r, Biome biome, uint64_t seed, uint32_t rid) {
    for (uint8_t res = 0; res < 16; ++res) {
        ResourceType rt = static_cast<ResourceType>(res);
        r.resource_regen[res]  = ResourceYieldTable::base_regen(static_cast<uint8_t>(biome), rt);
        r.resource_cap[res]    = ResourceYieldTable::base_capacity(static_cast<uint8_t>(biome), rt);
        r.resource_amount[res] = ResourceYieldTable::initial_amount(static_cast<uint8_t>(biome), rt, seed, rid);
    }
}

}  // namespace somni

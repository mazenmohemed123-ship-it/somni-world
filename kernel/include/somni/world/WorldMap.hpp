#pragma once
#include <somni/core/WorldState.hpp>
#include <cstdint>
#include <vector>
#include <optional>

namespace somni {

// ---------------------------------------------------------------------------
// Compact 2D pathfinding result
// ---------------------------------------------------------------------------
struct PathResult {
    bool success{false};
    std::vector<std::pair<int32_t, int32_t>> steps;  // (x, y) world cells
    float total_cost{0.0f};
};

// ---------------------------------------------------------------------------
// WorldMap: spatial query layer on top of WorldState
// Handles: adjacency, pathfinding, visibility, distance queries
// ---------------------------------------------------------------------------
class WorldMap {
public:
    explicit WorldMap(const WorldState& world);

    // Euclidean and Manhattan distance
    float   euclidean_dist(int32_t x0, int32_t y0, int32_t x1, int32_t y1) const;
    int32_t manhattan_dist(int32_t x0, int32_t y0, int32_t x1, int32_t y1) const;

    // A* pathfinding (navigable cells only)
    PathResult pathfind(int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                        uint32_t max_steps = 256) const;

    // Nearest cell with a given biome type
    std::optional<std::pair<int32_t,int32_t>>
    nearest_biome(int32_t cx, int32_t cy, Biome b, int32_t search_radius = 64) const;

    // All region IDs adjacent to a given region
    std::vector<uint32_t> adjacent_regions(uint32_t region_id) const;

    // Check line-of-sight (simple rayCast on elevation)
    bool has_line_of_sight(int32_t x0, int32_t y0, int32_t x1, int32_t y1) const;

    // Step one cell toward target (naive, no pathfinding — for fast NPCs)
    std::pair<int32_t, int32_t> step_toward(int32_t cx, int32_t cy,
                                             int32_t tx, int32_t ty) const;

    // Bounds check
    bool in_bounds(int32_t x, int32_t y) const {
        return x >= 0 && x < world_.config().width &&
               y >= 0 && y < world_.config().height;
    }

private:
    float movement_cost(int32_t x, int32_t y) const;

    const WorldState& world_;
};

}  // namespace somni

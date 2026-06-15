#include <somni/world/WorldMap.hpp>
#include <queue>
#include <cmath>
#include <unordered_map>
#include <algorithm>

namespace somni {

WorldMap::WorldMap(const WorldState& world) : world_(world) {}

float WorldMap::euclidean_dist(int32_t x0, int32_t y0, int32_t x1, int32_t y1) const {
    float dx = static_cast<float>(x1 - x0);
    float dy = static_cast<float>(y1 - y0);
    return std::sqrt(dx * dx + dy * dy);
}

int32_t WorldMap::manhattan_dist(int32_t x0, int32_t y0, int32_t x1, int32_t y1) const {
    return std::abs(x1 - x0) + std::abs(y1 - y0);
}

float WorldMap::movement_cost(int32_t x, int32_t y) const {
    const auto& c = world_.cell(x, y);
    if (!c.navigable) return 1e9f;
    float base = 1.0f;
    if (c.elevation > 0.6f) base += (c.elevation - 0.6f) * 5.0f;
    if (c.biome == Biome::SWAMP)  base += 1.5f;
    if (c.biome == Biome::JUNGLE) base += 0.5f;
    return base;
}

PathResult WorldMap::pathfind(int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                               uint32_t max_steps) const {
    PathResult result;
    if (!in_bounds(x0, y0) || !in_bounds(x1, y1)) return result;
    if (!world_.cell(x1, y1).navigable)            return result;

    using Node = std::pair<float, std::pair<int32_t, int32_t>>;  // (f, (x, y))
    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> open;
    std::unordered_map<int32_t, float>                g_cost;
    std::unordered_map<int32_t, std::pair<int32_t,int32_t>> came_from;

    int32_t W = world_.config().width;
    auto key  = [W](int32_t x, int32_t y) { return y * W + x; };
    auto h    = [this, x1, y1](int32_t x, int32_t y) {
        return euclidean_dist(x, y, x1, y1);
    };

    g_cost[key(x0, y0)] = 0.0f;
    open.push({h(x0, y0), {x0, y0}});

    static const int32_t dx[] = {-1, 1, 0, 0};
    static const int32_t dy[] = {0, 0, -1, 1};
    uint32_t steps = 0;

    while (!open.empty() && steps < max_steps) {
        auto [f, pos] = open.top(); open.pop();
        auto [cx, cy] = pos;
        ++steps;

        if (cx == x1 && cy == y1) {
            // Reconstruct path
            result.success = true;
            result.total_cost = g_cost[key(cx, cy)];
            auto [rx, ry] = pos;
            while (rx != x0 || ry != y0) {
                result.steps.push_back({rx, ry});
                auto prev = came_from[key(rx, ry)];
                rx = prev.first; ry = prev.second;
            }
            std::reverse(result.steps.begin(), result.steps.end());
            return result;
        }

        float g = g_cost[key(cx, cy)];
        for (int d = 0; d < 4; ++d) {
            int32_t nx = cx + dx[d], ny = cy + dy[d];
            if (!in_bounds(nx, ny)) continue;
            float cost = g + movement_cost(nx, ny);
            int32_t nk = key(nx, ny);
            if (g_cost.find(nk) == g_cost.end() || cost < g_cost[nk]) {
                g_cost[nk]    = cost;
                came_from[nk] = {cx, cy};
                open.push({cost + h(nx, ny), {nx, ny}});
            }
        }
    }
    return result;  // not found
}

std::optional<std::pair<int32_t,int32_t>>
WorldMap::nearest_biome(int32_t cx, int32_t cy, Biome b, int32_t search_radius) const {
    for (int32_t r = 1; r <= search_radius; ++r) {
        for (int32_t dy = -r; dy <= r; ++dy) {
            for (int32_t dx = -r; dx <= r; ++dx) {
                if (std::abs(dx) != r && std::abs(dy) != r) continue;
                int32_t nx = cx + dx, ny = cy + dy;
                if (!in_bounds(nx, ny)) continue;
                if (world_.cell(nx, ny).biome == b)
                    return std::make_pair(nx, ny);
            }
        }
    }
    return std::nullopt;
}

std::vector<uint32_t> WorldMap::adjacent_regions(uint32_t region_id) const {
    int32_t rw = world_.config().region_grid_w();
    int32_t rh = world_.config().region_grid_h();
    int32_t gx = static_cast<int32_t>(region_id % rw);
    int32_t gy = static_cast<int32_t>(region_id / rw);

    std::vector<uint32_t> adj;
    static const int32_t dx[] = {-1, 1, 0, 0};
    static const int32_t dy[] = {0, 0, -1, 1};
    for (int d = 0; d < 4; ++d) {
        int32_t nx = gx + dx[d], ny = gy + dy[d];
        if (nx >= 0 && nx < rw && ny >= 0 && ny < rh)
            adj.push_back(static_cast<uint32_t>(ny * rw + nx));
    }
    return adj;
}

bool WorldMap::has_line_of_sight(int32_t x0, int32_t y0, int32_t x1, int32_t y1) const {
    // Bresenham's line
    int32_t dx = std::abs(x1 - x0), dy = std::abs(y1 - y0);
    int32_t sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    int32_t err = dx - dy;
    float   max_elev = world_.cell(x0, y0).elevation;

    while (x0 != x1 || y0 != y1) {
        int32_t e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 <  dx) { err += dx; y0 += sy; }
        if (!in_bounds(x0, y0)) return false;
        float e = world_.cell(x0, y0).elevation;
        if (e > max_elev + 0.15f) return false;
    }
    return true;
}

std::pair<int32_t, int32_t>
WorldMap::step_toward(int32_t cx, int32_t cy, int32_t tx, int32_t ty) const {
    int32_t dx = tx - cx, dy = ty - cy;
    if (dx == 0 && dy == 0) return {cx, cy};

    int32_t sx = (dx > 0) ? 1 : (dx < 0) ? -1 : 0;
    int32_t sy = (dy > 0) ? 1 : (dy < 0) ? -1 : 0;

    int32_t nx = cx + sx, ny = cy + sy;
    if (in_bounds(nx, ny) && world_.cell(nx, ny).navigable) return {nx, ny};

    // Try axis-aligned fallback
    if (std::abs(dx) > std::abs(dy)) {
        nx = cx + sx; ny = cy;
        if (in_bounds(nx, ny) && world_.cell(nx, ny).navigable) return {nx, ny};
    }
    nx = cx; ny = cy + sy;
    if (in_bounds(nx, ny) && world_.cell(nx, ny).navigable) return {nx, ny};

    return {cx, cy};  // stuck
}

}  // namespace somni

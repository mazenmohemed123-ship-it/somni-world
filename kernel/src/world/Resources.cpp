#include <somni/world/Resources.hpp>
#include <algorithm>
#include <cmath>

namespace somni {

// ---------------------------------------------------------------------------
// Resource yield table — biome × resource → base rates
// All values tuned so simulation doesn't trivially over/under-supply
// ---------------------------------------------------------------------------

// biome_regen[biome][resource] = per-tick regen
static const float REGEN_TABLE[11][16] = {
//  GRAIN   MEAT    FISH    WATER   HERBS   TIMBER  STONE   IRON    GOLD    COAL    SALT    TOOLS   WEAP    TEXT    CURR    MANA
    {0.00f, 0.00f,  0.10f,  0.50f,  0.00f,  0.00f,  0.00f,  0.00f,  0.00f,  0.00f,  0.20f,  0.00f,  0.00f,  0.00f,  0.00f,  0.00f}, // OCEAN
    {0.02f, 0.03f,  0.00f,  0.05f,  0.02f,  0.01f,  0.03f,  0.02f,  0.00f,  0.01f,  0.00f,  0.00f,  0.00f,  0.00f,  0.00f,  0.00f}, // TUNDRA
    {0.03f, 0.05f,  0.01f,  0.08f,  0.04f,  0.12f,  0.04f,  0.03f,  0.01f,  0.02f,  0.00f,  0.00f,  0.00f,  0.00f,  0.00f,  0.00f}, // BOREAL
    {0.08f, 0.07f,  0.02f,  0.10f,  0.08f,  0.15f,  0.03f,  0.03f,  0.01f,  0.02f,  0.01f,  0.00f,  0.00f,  0.00f,  0.00f,  0.00f}, // TEMP_FOREST
    {0.15f, 0.08f,  0.00f,  0.06f,  0.06f,  0.03f,  0.02f,  0.01f,  0.00f,  0.01f,  0.01f,  0.00f,  0.00f,  0.00f,  0.00f,  0.00f}, // GRASSLAND
    {0.10f, 0.10f,  0.00f,  0.04f,  0.04f,  0.05f,  0.03f,  0.02f,  0.00f,  0.01f,  0.02f,  0.00f,  0.00f,  0.00f,  0.00f,  0.00f}, // SAVANNA
    {0.01f, 0.02f,  0.00f,  0.01f,  0.01f,  0.00f,  0.04f,  0.03f,  0.02f,  0.01f,  0.05f,  0.00f,  0.00f,  0.00f,  0.00f,  0.00f}, // DESERT
    {0.12f, 0.10f,  0.03f,  0.15f,  0.12f,  0.18f,  0.01f,  0.02f,  0.01f,  0.01f,  0.00f,  0.00f,  0.00f,  0.00f,  0.00f,  0.02f}, // JUNGLE
    {0.08f, 0.06f,  0.08f,  0.20f,  0.10f,  0.10f,  0.01f,  0.01f,  0.00f,  0.01f,  0.03f,  0.00f,  0.00f,  0.00f,  0.00f,  0.01f}, // SWAMP
    {0.01f, 0.04f,  0.00f,  0.03f,  0.02f,  0.02f,  0.10f,  0.08f,  0.03f,  0.05f,  0.02f,  0.00f,  0.00f,  0.00f,  0.00f,  0.00f}, // MOUNTAINS
    {0.00f, 0.01f,  0.02f,  0.08f,  0.00f,  0.00f,  0.02f,  0.01f,  0.00f,  0.00f,  0.10f,  0.00f,  0.00f,  0.00f,  0.00f,  0.00f}, // ICE_SHEET
};

// biome_cap[biome][resource] = maximum stockpile
static const float CAP_TABLE[11][16] = {
//  GRAIN   MEAT    FISH    WATER   HERBS   TIMBER  STONE   IRON    GOLD    COAL    SALT    TOOLS   WEAP    TEXT    CURR    MANA
    {  0.0,   0.0,  500.0, 9999.0,   0.0,    0.0,    0.0,    0.0,    0.0,    0.0,  200.0,   0.0,    0.0,    0.0,    0.0,    0.0}, // OCEAN
    { 50.0,  80.0,    0.0,  100.0,  40.0,   30.0,  200.0,   80.0,   10.0,   40.0,    0.0,   0.0,    0.0,    0.0,    0.0,    0.0}, // TUNDRA
    { 80.0, 150.0,   30.0,  200.0, 100.0,  400.0,  250.0,  120.0,   30.0,   80.0,    0.0,   0.0,    0.0,    0.0,    0.0,    0.0}, // BOREAL
    {200.0, 180.0,   60.0,  300.0, 200.0,  500.0,  200.0,  120.0,   40.0,   80.0,   30.0,   0.0,    0.0,    0.0,    0.0,    0.0}, // TEMP_FOREST
    {400.0, 200.0,    0.0,  150.0, 150.0,   80.0,  150.0,   60.0,   20.0,   40.0,   40.0,   0.0,    0.0,    0.0,    0.0,    0.0}, // GRASSLAND
    {300.0, 250.0,    0.0,  100.0, 100.0,  120.0,  150.0,   80.0,   20.0,   40.0,   60.0,   0.0,    0.0,    0.0,    0.0,    0.0}, // SAVANNA
    { 20.0,  50.0,    0.0,   30.0,  20.0,    5.0,  200.0,  150.0,   80.0,   50.0,  200.0,   0.0,    0.0,    0.0,    0.0,    0.0}, // DESERT
    {300.0, 250.0,   80.0,  400.0, 350.0,  600.0,  100.0,   80.0,   30.0,   40.0,    0.0,   0.0,    0.0,    0.0,    0.0,   50.0}, // JUNGLE
    {200.0, 150.0,  200.0,  600.0, 250.0,  300.0,   80.0,   40.0,   10.0,   40.0,   80.0,   0.0,    0.0,    0.0,    0.0,   20.0}, // SWAMP
    { 20.0, 100.0,    0.0,   80.0,  50.0,   50.0,  500.0,  350.0,  100.0,  200.0,   60.0,   0.0,    0.0,    0.0,    0.0,    0.0}, // MOUNTAINS
    {  0.0,  20.0,   50.0,  200.0,   0.0,    0.0,  100.0,   40.0,    0.0,    0.0,  300.0,   0.0,    0.0,    0.0,    0.0,    0.0}, // ICE_SHEET
};

float ResourceYieldTable::base_regen(uint8_t biome, ResourceType res) {
    if (biome >= 11) return 0.0f;
    return REGEN_TABLE[biome][static_cast<uint8_t>(res)];
}

float ResourceYieldTable::base_capacity(uint8_t biome, ResourceType res) {
    if (biome >= 11) return 0.0f;
    return CAP_TABLE[biome][static_cast<uint8_t>(res)];
}

float ResourceYieldTable::initial_amount(uint8_t biome, ResourceType res,
                                          uint64_t seed, uint32_t region_id) {
    float cap = base_capacity(biome, res);
    if (cap <= 0.0f) return 0.0f;
    // Deterministic initial fill 40%–80% of cap
    uint64_t h = seed ^ (static_cast<uint64_t>(region_id) * 2654435761ULL)
                      ^ (static_cast<uint64_t>(res) * 40503ULL);
    float frac = 0.4f + (h & 0xFFFF) / 65535.0f * 0.4f;
    return cap * frac;
}

// ---------------------------------------------------------------------------
// InventoryComponent
// ---------------------------------------------------------------------------

float InventoryComponent::get(ResourceType r) const {
    for (uint8_t i = 0; i < slot_count; ++i)
        if (types[i] == r) return amounts[i];
    return 0.0f;
}

bool InventoryComponent::add(ResourceType r, float amount) {
    // Find existing slot
    for (uint8_t i = 0; i < slot_count; ++i) {
        if (types[i] == r) { amounts[i] += amount; return true; }
    }
    // New slot
    if (slot_count >= SLOTS) return false;
    types[slot_count]   = r;
    amounts[slot_count] = amount;
    ++slot_count;
    return true;
}

float InventoryComponent::remove(ResourceType r, float amount) {
    for (uint8_t i = 0; i < slot_count; ++i) {
        if (types[i] == r) {
            float taken = std::min(amounts[i], amount);
            amounts[i] -= taken;
            if (amounts[i] < 0.001f) {
                // Remove slot
                types[i]   = types[slot_count - 1];
                amounts[i] = amounts[slot_count - 1];
                --slot_count;
            }
            return taken;
        }
    }
    return 0.0f;
}

bool InventoryComponent::has(ResourceType r, float min) const {
    return get(r) >= min;
}

}  // namespace somni

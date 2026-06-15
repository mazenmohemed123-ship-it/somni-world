#pragma once
#include <array>
#include <cstdint>
#include <string_view>

namespace somni {

// ---------------------------------------------------------------------------
// Resource type enum — 16 types, indexable as uint8
// ---------------------------------------------------------------------------
enum class ResourceType : uint8_t {
    // Biological
    FOOD_GRAIN   = 0,
    FOOD_MEAT    = 1,
    FOOD_FISH    = 2,
    WATER_FRESH  = 3,
    HERBS        = 4,
    TIMBER       = 5,

    // Minerals
    STONE        = 6,
    IRON_ORE     = 7,
    GOLD         = 8,
    COAL         = 9,
    SALT         = 10,

    // Manufactured (produced by NPCs / factions)
    TOOLS        = 11,
    WEAPONS      = 12,
    TEXTILES     = 13,
    CURRENCY     = 14,

    // Special
    MANA         = 15,   // for fantasy worlds; 0 by default in realistic modes

    COUNT        = 16
};

constexpr uint8_t RESOURCE_COUNT = static_cast<uint8_t>(ResourceType::COUNT);

constexpr std::string_view resource_name(ResourceType r) {
    switch (r) {
        case ResourceType::FOOD_GRAIN:  return "food_grain";
        case ResourceType::FOOD_MEAT:   return "food_meat";
        case ResourceType::FOOD_FISH:   return "food_fish";
        case ResourceType::WATER_FRESH: return "water_fresh";
        case ResourceType::HERBS:       return "herbs";
        case ResourceType::TIMBER:      return "timber";
        case ResourceType::STONE:       return "stone";
        case ResourceType::IRON_ORE:    return "iron_ore";
        case ResourceType::GOLD:        return "gold";
        case ResourceType::COAL:        return "coal";
        case ResourceType::SALT:        return "salt";
        case ResourceType::TOOLS:       return "tools";
        case ResourceType::WEAPONS:     return "weapons";
        case ResourceType::TEXTILES:    return "textiles";
        case ResourceType::CURRENCY:    return "currency";
        case ResourceType::MANA:        return "mana";
        default:                        return "unknown";
    }
}

// ---------------------------------------------------------------------------
// Resource biome yield table — base regen per tick per region
// Indexed as [biome][resource]
// ---------------------------------------------------------------------------
struct ResourceYieldTable {
    // Returns the base regeneration rate for a resource in a biome
    static float base_regen(uint8_t biome, ResourceType res);
    // Returns the base capacity for a resource in a biome
    static float base_capacity(uint8_t biome, ResourceType res);
    // Returns the initial spawn amount [0..capacity]
    static float initial_amount(uint8_t biome, ResourceType res, uint64_t seed, uint32_t region_id);
};

// ---------------------------------------------------------------------------
// NPC inventory — small fixed array, no heap allocation
// 8 slots: NPCs carry limited resources
// ---------------------------------------------------------------------------
struct InventoryComponent {
    static constexpr uint8_t SLOTS = 8;
    std::array<ResourceType, SLOTS> types{};
    std::array<float,        SLOTS> amounts{};
    uint8_t slot_count{0};

    float   get(ResourceType r) const;
    bool    add(ResourceType r, float amount);    // returns false if full
    float   remove(ResourceType r, float amount); // returns amount actually removed
    bool    has(ResourceType r, float min = 0.01f) const;
    bool    is_full()  const { return slot_count >= SLOTS; }
    bool    is_empty() const { return slot_count == 0; }
};

}  // namespace somni

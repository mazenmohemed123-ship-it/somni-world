#pragma once
#include <cstdint>
#include <cmath>
#include <type_traits>

namespace somni {

// ===========================================================================
// Q16.16 Fixed-Point Arithmetic
// Range: ±32767.9999847 with precision 0.0000152 (1/65536)
// Fully deterministic: no IEEE rounding variance, same on all platforms
// ===========================================================================
struct Fixed32 {
    int32_t raw{0};   // stored as integer * 65536

    static constexpr int32_t  FRAC_BITS = 16;
    static constexpr int32_t  SCALE     = 1 << FRAC_BITS;   // 65536
    // Defined out-of-line below (Fixed32 is incomplete here, so cannot
    // initialize members of its own type in-class).
    static const Fixed32 ZERO;
    static const Fixed32 ONE;
    static const Fixed32 HALF;

    // Constructors
    constexpr Fixed32() noexcept = default;
    constexpr explicit Fixed32(int32_t integer) noexcept : raw(integer << FRAC_BITS) {}
    constexpr explicit Fixed32(float f) noexcept
        : raw(static_cast<int32_t>(f * SCALE)) {}

    static constexpr Fixed32 from_raw(int32_t r) noexcept {
        Fixed32 f; f.raw = r; return f;
    }

    // Conversion
    constexpr float   to_float()   const noexcept { return static_cast<float>(raw) / SCALE; }
    constexpr int32_t to_int()     const noexcept { return raw >> FRAC_BITS; }
    constexpr int32_t floor()      const noexcept { return raw >> FRAC_BITS; }
    constexpr int32_t ceil()       const noexcept { return (raw + SCALE - 1) >> FRAC_BITS; }

    // Arithmetic — all branchless, deterministic
    constexpr Fixed32 operator+(Fixed32 o) const noexcept { return from_raw(raw + o.raw); }
    constexpr Fixed32 operator-(Fixed32 o) const noexcept { return from_raw(raw - o.raw); }
    constexpr Fixed32 operator-()          const noexcept { return from_raw(-raw); }

    constexpr Fixed32 operator*(Fixed32 o) const noexcept {
        return from_raw(static_cast<int32_t>(
            (static_cast<int64_t>(raw) * o.raw) >> FRAC_BITS));
    }
    constexpr Fixed32 operator/(Fixed32 o) const noexcept {
        return from_raw(static_cast<int32_t>(
            (static_cast<int64_t>(raw) << FRAC_BITS) / o.raw));
    }

    Fixed32& operator+=(Fixed32 o) noexcept { raw += o.raw; return *this; }
    Fixed32& operator-=(Fixed32 o) noexcept { raw -= o.raw; return *this; }
    Fixed32& operator*=(Fixed32 o) noexcept { *this = *this * o; return *this; }
    Fixed32& operator/=(Fixed32 o) noexcept { *this = *this / o; return *this; }

    // Comparisons
    constexpr bool operator==(Fixed32 o) const noexcept { return raw == o.raw; }
    constexpr bool operator!=(Fixed32 o) const noexcept { return raw != o.raw; }
    constexpr bool operator< (Fixed32 o) const noexcept { return raw <  o.raw; }
    constexpr bool operator<=(Fixed32 o) const noexcept { return raw <= o.raw; }
    constexpr bool operator> (Fixed32 o) const noexcept { return raw >  o.raw; }
    constexpr bool operator>=(Fixed32 o) const noexcept { return raw >= o.raw; }

    // Math utilities (all integer-based, deterministic)
    static Fixed32 abs(Fixed32 x)       noexcept { return x.raw < 0 ? from_raw(-x.raw) : x; }
    static Fixed32 min(Fixed32 a, Fixed32 b) noexcept { return a < b ? a : b; }
    static Fixed32 max(Fixed32 a, Fixed32 b) noexcept { return a > b ? a : b; }
    static Fixed32 clamp(Fixed32 v, Fixed32 lo, Fixed32 hi) noexcept {
        return min(max(v, lo), hi);
    }

    // Integer square root (Newton-Raphson, deterministic)
    static Fixed32 sqrt(Fixed32 x) noexcept {
        if (x.raw <= 0) return ZERO;
        // Shift input left by FRAC_BITS to preserve fractional precision
        int64_t val = static_cast<int64_t>(x.raw) << FRAC_BITS;
        int64_t guess = val >> 1;
        if (guess == 0) return ZERO;
        for (int i = 0; i < 8; ++i) {
            guess = (guess + val / guess) >> 1;
        }
        return from_raw(static_cast<int32_t>(guess));
    }

    // Linear interpolation: lerp(a, b, t) where t in [0,1]
    static Fixed32 lerp(Fixed32 a, Fixed32 b, Fixed32 t) noexcept {
        return a + (b - a) * t;
    }
};

// Out-of-line definitions of the named constants (Fixed32 is now complete).
// inline => single definition across all translation units (C++17).
inline const Fixed32 Fixed32::ZERO = Fixed32::from_raw(0);
inline const Fixed32 Fixed32::ONE  = Fixed32::from_raw(Fixed32::SCALE);
inline const Fixed32 Fixed32::HALF = Fixed32::from_raw(Fixed32::SCALE / 2);

// Scalar operations
inline Fixed32 operator*(int32_t s, Fixed32 f) noexcept {
    return Fixed32::from_raw(s * f.raw);
}
inline Fixed32 operator*(Fixed32 f, int32_t s) noexcept {
    return Fixed32::from_raw(f.raw * s);
}

// ===========================================================================
// PCG32 — Permuted Congruential Generator
// Deterministic, seeded, high quality, period 2^64
// All simulation randomness MUST go through this (no std::rand, no rand())
// ===========================================================================
struct PCG32 {
    uint64_t state{0};
    uint64_t inc{1};

    PCG32() = default;

    explicit PCG32(uint64_t seed, uint64_t seq = 1) noexcept {
        state = 0;
        inc   = (seq << 1u) | 1u;
        next_u32();
        state += seed;
        next_u32();
    }

    uint32_t next_u32() noexcept {
        uint64_t old = state;
        state = old * 6364136223846793005ULL + inc;
        uint32_t xsh = static_cast<uint32_t>(((old >> 18u) ^ old) >> 27u);
        uint32_t rot = static_cast<uint32_t>(old >> 59u);
        return (xsh >> rot) | (xsh << ((~rot + 1u) & 31u));
    }

    // [lo, hi] inclusive
    int32_t range_i(int32_t lo, int32_t hi) noexcept {
        uint32_t span = static_cast<uint32_t>(hi - lo + 1);
        return lo + static_cast<int32_t>(next_u32() % span);
    }

    // Fixed32 in [0, 1)
    Fixed32 next_fixed() noexcept {
        // Use top 16 bits as fractional part → range [0, 1)
        return Fixed32::from_raw(static_cast<int32_t>(next_u32() >> 16));
    }

    // Fixed32 in [lo, hi]
    Fixed32 range_f(Fixed32 lo, Fixed32 hi) noexcept {
        return lo + (hi - lo) * next_fixed();
    }

    // Boolean with probability p (Fixed32 in [0,1])
    bool chance(Fixed32 p) noexcept {
        return next_fixed() < p;
    }
};

// ===========================================================================
// GlobalRNGPool — one seeded PCG32 per subsystem, no shared global state
// ===========================================================================
enum class RNGDomain : uint8_t {
    TERRAIN    = 0,
    AGENTS     = 1,
    ECONOMY    = 2,
    CONFLICT   = 3,
    EVENTS     = 4,
    COUNT      = 5
};

class RNGPool {
public:
    explicit RNGPool(uint64_t world_seed) {
        // Each domain gets a unique stream from the same seed
        for (uint8_t i = 0; i < static_cast<uint8_t>(RNGDomain::COUNT); ++i) {
            rngs_[i] = PCG32(world_seed, static_cast<uint64_t>(i) + 1);
        }
    }

    PCG32& get(RNGDomain d) { return rngs_[static_cast<uint8_t>(d)]; }

private:
    PCG32 rngs_[static_cast<uint8_t>(RNGDomain::COUNT)];
};

// ===========================================================================
// Deterministic hash — for tick-seeded decisions without advancing RNG state
// Use when you need a one-off value that must be same across replay
// ===========================================================================
inline uint32_t det_hash(uint64_t tick, uint32_t id_a, uint32_t id_b = 0) noexcept {
    uint64_t h = 14695981039346656037ULL;
    h ^= tick;         h *= 1099511628211ULL;
    h ^= id_a;         h *= 1099511628211ULL;
    h ^= id_b;         h *= 1099511628211ULL;
    h ^= h >> 33;
    return static_cast<uint32_t>(h);
}

inline Fixed32 det_hash_f(uint64_t tick, uint32_t id) noexcept {
    uint32_t h = det_hash(tick, id);
    return Fixed32::from_raw(static_cast<int32_t>(h >> 16)); // [0, 1)
}

}  // namespace somni

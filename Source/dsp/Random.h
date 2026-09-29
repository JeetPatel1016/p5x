// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#pragma once

#include <cstdint>

namespace p5x
{
// xorshift128+ (Vigna, "Further scramblings of Marsaglia's xorshift generators"), seeded through
// splitmix64 so any 64-bit seed gives a well-mixed state. All randomness in P5X comes from here
// (see 00-architecture.md § Randomness): same seed + same input = same output.
class Random
{
public:
    explicit Random (uint64_t seed = 1) noexcept { setSeed (seed); }

    void setSeed (uint64_t seed) noexcept
    {
        uint64_t state = seed;
        s0 = splitMix (state);
        s1 = splitMix (state);

        if (s0 == 0 && s1 == 0)
            s1 = 1;
    }

    uint64_t nextUInt64() noexcept
    {
        uint64_t x = s0;
        const uint64_t y = s1;
        s0 = y;
        x ^= x << 23;
        s1 = x ^ y ^ (x >> 17) ^ (y >> 26);
        return s1 + y;
    }

    // Uniform in [0, 1).
    double nextDouble() noexcept { return static_cast<double> (nextUInt64() >> 11) * 0x1.0p-53; }

    // Uniform in [-1, 1).
    float nextBipolar() noexcept { return static_cast<float> (nextDouble() * 2.0 - 1.0); }

    // A seed for sub-stream `stream` (e.g. voice index) derived from an instance seed.
    static uint64_t derive (uint64_t seed, uint64_t stream) noexcept
    {
        uint64_t state = seed ^ (stream * 0xD1B54A32D192ED03ull);
        return splitMix (state);
    }

private:
    static uint64_t splitMix (uint64_t& state) noexcept
    {
        uint64_t z = (state += 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }

    uint64_t s0 = 0, s1 = 0;
};
} // namespace p5x

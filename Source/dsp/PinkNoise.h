// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#pragma once

#include "dsp/Random.h"

#include <array>
#include <cmath>
#include <cstdint>

namespace p5x::dsp
{
// Wheel-Mod's global pink noise (05-modulation.md § Wheel-Mod): uniform white noise through Paul
// Kellet's "economy" −3 dB/oct filter (three one-pole sections plus a direct term, from his
// published coefficients), scaled so the output RMS is about 0.35.
class PinkNoise
{
public:
    static constexpr double kTargetRms = 0.35;

    void prepare (uint64_t seed) noexcept
    {
        random.setSeed (seed);
        state = {};
        scale = (float) (kTargetRms / unscaledRms());
    }

    float process() noexcept
    {
        const double white = random.nextBipolar();
        double sum = white * kDirect;

        for (size_t k = 0; k < kPoles.size(); ++k)
        {
            state[k] = kPoles[k] * state[k] + white * kGains[k];
            sum += state[k];
        }

        return (float) sum * scale;
    }

    // RMS of the filter output for uniform white input in [−1, 1] (variance 1/3), from the sum of
    // the squared impulse response: h[0] = d + Σg, h[n] = Σ g·aⁿ.
    static double unscaledRms() noexcept
    {
        double energy = kDirect * kDirect;

        for (size_t k = 0; k < kPoles.size(); ++k)
        {
            energy += 2.0 * kDirect * kGains[k];

            for (size_t l = 0; l < kPoles.size(); ++l)
                energy += kGains[k] * kGains[l] / (1.0 - kPoles[k] * kPoles[l]);
        }

        return std::sqrt (energy / 3.0);
    }

private:
    static constexpr std::array<double, 3> kPoles { 0.99765, 0.96300, 0.57000 };
    static constexpr std::array<double, 3> kGains { 0.0990460, 0.2965164, 1.0526913 };
    static constexpr double kDirect = 0.1848;

    Random random;
    std::array<double, 3> state {};
    float scale = 1.0f;
};
} // namespace p5x::dsp

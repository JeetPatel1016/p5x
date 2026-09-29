// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#pragma once

#include <cmath>

namespace p5x::dsp
{
// Output stage safety clipper (06-voices.md § Output stage): identity up to ±0.8, then a tanh knee
// that is continuous in value and slope and never exceeds ±1.0. Keeps a runaway patch (or a big
// chord at full master volume) from hard-clipping at the host or sound card.
struct SafetyClipper
{
    static constexpr float kThreshold = 0.8f;
    static constexpr float kHeadroom = 1.0f - kThreshold;

    static float process (float x) noexcept
    {
        const float magnitude = std::abs (x);

        if (magnitude <= kThreshold)
            return x;

        const float shaped = kThreshold + kHeadroom * std::tanh ((magnitude - kThreshold) / kHeadroom);
        return x < 0.0f ? -shaped : shaped;
    }

    // Processes a block in place; returns true if any sample went past the threshold.
    static bool processBlock (float* samples, int numSamples) noexcept
    {
        bool engaged = false;

        for (int i = 0; i < numSamples; ++i)
        {
            if (std::abs (samples[i]) > kThreshold)
            {
                samples[i] = process (samples[i]);
                engaged = true;
            }
        }

        return engaged;
    }
};
} // namespace p5x::dsp

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

// DC blocker on the summed output (06-voices.md § Output stage): one-pole high-pass at 5 Hz. One for
// the whole mix rather than one per voice: the filter is linear, so this is the same filtering, but a
// voice that stops can't cut off a half-decayed blocker tail (that step was an audible click).
class DcBlocker
{
public:
    static constexpr double kCutoffHz = 5.0;

    void prepare (double sampleRate) noexcept
    {
        coeff = std::exp (-2.0 * 3.14159265358979323846 * kCutoffHz / (sampleRate > 0.0 ? sampleRate : 48000.0));
        reset();
    }

    void reset() noexcept { in1 = out1 = 0.0; }

    void processBlock (float* samples, int numSamples) noexcept
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const double x = samples[i];
            out1 = x - in1 + coeff * out1;
            in1 = x;
            samples[i] = (float) out1;
        }
    }

private:
    double coeff = 0.0, in1 = 0.0, out1 = 0.0;
};
} // namespace p5x::dsp

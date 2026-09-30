// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
// 03-filter.md § Tests (values follow the specified model; see DECISIONS.md).
#include "dsp/LadderFilterCEM.h"
#include "dsp/Random.h"
#include "support/Spectrum.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <numbers>

using Catch::Approx;
using p5x::dsp::LadderFilterCEM;

namespace
{
constexpr double kRate = 96000.0;

// Steady-state gain in dB of a small sine through the filter (small enough to keep tanh linear).
double gainDb (double freq, float cutoff, float res)
{
    LadderFilterCEM filter;
    filter.prepare (kRate);
    constexpr double amplitude = 1.0e-3;
    const int settle = (int) kRate, measure = (int) (0.2 * kRate);
    double peak = 0.0;

    for (int i = 0; i < settle + measure; ++i)
    {
        const double x = amplitude * std::sin (2.0 * std::numbers::pi * freq * i / kRate);
        const float y = filter.process ((float) x, cutoff, res);

        if (i >= settle)
            peak = std::max (peak, (double) std::abs (y));
    }

    return p5x::test::toDb (peak / amplitude);
}
} // namespace

TEST_CASE ("Filter: res 0, fc 1 kHz: -3 dB point at 435 Hz", "[dsp][filter]")
{
    const double reference = gainDb (10.0, 1000.0f, 0.0f);
    double lo = 200.0, hi = 1000.0;

    for (int i = 0; i < 30; ++i)
    {
        const double mid = 0.5 * (lo + hi);
        (gainDb (mid, 1000.0f, 0.0f) - reference > -3.0103 ? lo : hi) = mid;
    }

    REQUIRE (0.5 * (lo + hi) == Approx (435.0).epsilon (0.05));
}

TEST_CASE ("Filter: slope between 2 and 4 kHz is 21.2 dB", "[dsp][filter]")
{
    REQUIRE (gainDb (2000.0, 1000.0f, 0.0f) - gainDb (4000.0, 1000.0f, 0.0f) == Approx (21.2).margin (1.0));
}

TEST_CASE ("Filter: passband loss at res 0.9 is 13.1 dB (not compensated)", "[dsp][filter]")
{
    REQUIRE (gainDb (100.0, 1000.0f, 0.0f) - gainDb (100.0, 1000.0f, 0.9f) == Approx (13.1).margin (1.0));
}

TEST_CASE ("Filter: self-oscillates at fc from silence and settles", "[dsp][filter][slow]")
{
    for (float fc : { 100.0f, 1000.0f, 8000.0f })
    {
        LadderFilterCEM filter;
        filter.prepare (kRate); // reset() queues the 1e-6 start-up excitation
        const auto total = (size_t) (9.0 * kRate);
        std::vector<float> y (total);

        for (size_t i = 0; i < total; ++i)
            y[i] = filter.process (0.0f, fc, 1.0f);

        INFO ("fc " << fc);

        for (float s : y)
            REQUIRE (std::isfinite (s));

        // Frequency from rising zero crossings in the 8–9 s window.
        const auto start = (size_t) (8.0 * kRate);
        int crossings = 0;
        size_t first = 0, last = 0;

        for (size_t i = start + 1; i < total; ++i)
        {
            if (y[i - 1] < 0.0f && y[i] >= 0.0f)
            {
                if (crossings == 0)
                    first = i;

                last = i;
                ++crossings;
            }
        }

        REQUIRE (crossings > 10);
        REQUIRE ((crossings - 1) * kRate / (double) (last - first) == Approx (fc).epsilon (0.03));

        const auto half = (size_t) (8.5 * kRate);
        const double a = p5x::test::rms (y, start, half), b = p5x::test::rms (y, half, total);
        REQUIRE (a > 1.0e-3);
        REQUIRE (p5x::test::toDb (b / a) == Approx (0.0).margin (0.5));
    }
}

TEST_CASE ("Filter: fast exponential cutoff sweeps at res 0.95 stay bounded", "[dsp][filter]")
{
    // One full 20 Hz → 20 kHz → 20 Hz sweep every 1 ms, white-noise input (03-filter.md).
    LadderFilterCEM filter;
    filter.prepare (kRate);
    p5x::Random random (8);
    const int period = (int) (0.001 * kRate);

    for (int i = 0; i < (int) (2.0 * kRate); ++i)
    {
        const double t = (double) (i % period) / period;
        const double tri = t < 0.5 ? 2.0 * t : 2.0 - 2.0 * t;
        const auto cutoff = (float) (20.0 * std::pow (1000.0, tri));
        const float y = filter.process (random.nextBipolar(), cutoff, 0.95f);

        REQUIRE (std::isfinite (y));
        REQUIRE (std::abs (y) < 4.0f);
    }
}

TEST_CASE ("Filter: noise with random cutoff and resonance each sample for 10 s", "[dsp][filter][slow]")
{
    LadderFilterCEM filter;
    filter.prepare (kRate);
    p5x::Random random (9);

    for (int i = 0; i < (int) (10.0 * kRate); ++i)
    {
        const auto cutoff = (float) (random.nextDouble() * 60000.0 - 5000.0); // includes out-of-range values
        const auto res = (float) (random.nextDouble() * 1.4 - 0.2);
        const float y = filter.process (random.nextBipolar(), cutoff, res);

        REQUIRE (std::isfinite (y));
        REQUIRE (std::abs (y) < 4.0f);
    }
}

TEST_CASE ("Filter: feedback amount", "[dsp][filter]")
{
    REQUIRE (LadderFilterCEM::feedbackFor (0.0f) == 0.0f);
    REQUIRE (LadderFilterCEM::feedbackFor (1.0f) == Approx (4.08f));
    REQUIRE (LadderFilterCEM::feedbackFor (0.9f) == Approx (4.0f * std::pow (0.9f, 1.1f) * 1.02f));
}

// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#include "dsp/OutputStage.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

using Catch::Approx;
using p5x::dsp::SafetyClipper;

TEST_CASE ("Safety clipper: identity up to +-0.8", "[dsp][output]")
{
    for (float x : { 0.0f, 0.1f, -0.5f, 0.79f, 0.8f, -0.8f })
        REQUIRE (SafetyClipper::process (x) == x);
}

TEST_CASE ("Safety clipper: continuous in value and slope at the threshold", "[dsp][output]")
{
    constexpr float h = 1.0e-4f;
    const float below = SafetyClipper::process (0.8f - h);
    const float above = SafetyClipper::process (0.8f + h);

    REQUIRE (above - 0.8f == Approx (0.8f - below).margin (1.0e-6)); // slope ≈ 1 on both sides
    REQUIRE (SafetyClipper::process (-0.8f - h) == Approx (-above));  // odd-symmetric
}

TEST_CASE ("Safety clipper: never exceeds +-1 and stays monotonic", "[dsp][output]")
{
    float previous = SafetyClipper::process (0.0f);

    for (float x = 0.0f; x < 50.0f; x += 0.01f)
    {
        const float y = SafetyClipper::process (x);
        REQUIRE (y <= 1.0f);
        REQUIRE (y >= previous);
        REQUIRE (SafetyClipper::process (-x) == -y);
        previous = y;
    }

    REQUIRE (SafetyClipper::process (1.5f) > 0.99f); // big overshoots land just under the ceiling
}

TEST_CASE ("Safety clipper: block processing reports engagement", "[dsp][output]")
{
    std::vector<float> quiet { 0.1f, -0.7f, 0.8f };
    REQUIRE_FALSE (SafetyClipper::processBlock (quiet.data(), (int) quiet.size()));
    REQUIRE (quiet == std::vector<float> { 0.1f, -0.7f, 0.8f });

    std::vector<float> loud { 0.1f, 1.4f, -2.0f };
    REQUIRE (SafetyClipper::processBlock (loud.data(), (int) loud.size()));
    REQUIRE (loud[0] == 0.1f);
    REQUIRE (loud[1] < 1.0f);
    REQUIRE (loud[2] > -1.0f);
}

TEST_CASE ("DC blocker: removes DC, passes audio, -3 dB near 5 Hz", "[dsp][output]")
{
    constexpr double rate = 48000.0;
    p5x::dsp::DcBlocker blocker;
    blocker.prepare (rate);

    std::vector<float> dc ((size_t) rate * 2, 0.5f);
    blocker.processBlock (dc.data(), (int) dc.size());
    REQUIRE (std::abs (dc.back()) < 1.0e-4f);

    auto gainAt = [&] (double hz)
    {
        blocker.reset();
        std::vector<float> x ((size_t) rate * 4);

        for (size_t i = 0; i < x.size(); ++i)
            x[i] = (float) std::sin (2.0 * 3.141592653589793 * hz * (double) i / rate);

        blocker.processBlock (x.data(), (int) x.size());
        float peak = 0.0f;

        for (size_t i = x.size() / 2; i < x.size(); ++i)
            peak = std::max (peak, std::abs (x[i]));

        return peak;
    };

    REQUIRE (gainAt (100.0) > 0.99f);
    REQUIRE (gainAt (5.0) == Catch::Approx (0.707).margin (0.03));
}

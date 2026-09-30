// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#include "dsp/OutputStage.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

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

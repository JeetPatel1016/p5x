// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#include "dsp/Smoother.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using Catch::Approx;

TEST_CASE ("LinearSmoother: reaches the target in exactly the ramp time", "[dsp][smoother]")
{
    p5x::dsp::LinearSmoother s;
    s.prepare (48000.0, 20.0); // Lin 20 = 960 samples
    s.reset (0.0f);
    s.setTarget (1.0f);

    float value = 0.0f;

    for (int i = 0; i < 959; ++i)
        value = s.next();

    REQUIRE (value < 1.0f);
    REQUIRE (value == Approx (959.0f / 960.0f).margin (1.0e-4));
    REQUIRE (s.next() == 1.0f);
    REQUIRE_FALSE (s.isSmoothing());
    REQUIRE (s.next() == 1.0f);
}

TEST_CASE ("LinearSmoother: a new target mid-ramp restarts from the current value", "[dsp][smoother]")
{
    p5x::dsp::LinearSmoother s;
    s.prepare (1000.0, 10.0); // 10 samples
    s.reset (0.0f);
    s.setTarget (1.0f);

    for (int i = 0; i < 5; ++i)
        s.next();

    const float midway = s.getCurrent();
    s.setTarget (0.0f);
    const float first = s.next();

    REQUIRE (first < midway);
    REQUIRE (first == Approx (midway - midway / 10.0f).margin (1.0e-6));
}

TEST_CASE ("LinearSmoother: reset jumps without ramping", "[dsp][smoother]")
{
    p5x::dsp::LinearSmoother s;
    s.prepare (44100.0, 5.0);
    s.reset (0.25f);

    REQUIRE (s.next() == 0.25f);
    REQUIRE_FALSE (s.isSmoothing());
}

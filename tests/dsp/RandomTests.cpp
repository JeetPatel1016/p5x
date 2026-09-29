// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#include "dsp/Random.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE ("Random: same seed gives the same sequence", "[dsp][random]")
{
    p5x::Random a (1234), b (1234);

    for (int i = 0; i < 1000; ++i)
        REQUIRE (a.nextUInt64() == b.nextUInt64());
}

TEST_CASE ("Random: different seeds give different sequences", "[dsp][random]")
{
    p5x::Random a (1), b (2);
    int equal = 0;

    for (int i = 0; i < 100; ++i)
        equal += a.nextUInt64() == b.nextUInt64() ? 1 : 0;

    REQUIRE (equal == 0);
}

TEST_CASE ("Random: ranges", "[dsp][random]")
{
    p5x::Random r (99);

    for (int i = 0; i < 100000; ++i)
    {
        const double d = r.nextDouble();
        REQUIRE (d >= 0.0);
        REQUIRE (d < 1.0);

        const float b = r.nextBipolar();
        REQUIRE (b >= -1.0f);
        REQUIRE (b < 1.0f);
    }
}

TEST_CASE ("Random: derived voice seeds differ per stream and are stable", "[dsp][random]")
{
    REQUIRE (p5x::Random::derive (42, 0) != p5x::Random::derive (42, 1));
    REQUIRE (p5x::Random::derive (42, 3) == p5x::Random::derive (42, 3));
    REQUIRE (p5x::Random::derive (42, 3) != p5x::Random::derive (43, 3));
}

TEST_CASE ("Random: a zero seed still produces output", "[dsp][random]")
{
    p5x::Random r (0);
    uint64_t combined = 0;

    for (int i = 0; i < 10; ++i)
        combined |= r.nextUInt64();

    REQUIRE (combined != 0);
}

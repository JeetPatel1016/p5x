// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
// 04-envelopes.md § Tests.
#include "dsp/Envelope.h"
#include "dsp/Smoother.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

using Catch::Approx;
using p5x::dsp::Envelope;
using p5x::dsp::EnvParams;
using Stage = Envelope::Stage;

namespace
{
constexpr double kRate = 96000.0;

// Runs until `stage` is left; returns the number of samples spent in it.
int samplesInStage (Envelope& env, const EnvParams& p, Stage stage, int limit = 10'000'000)
{
    int n = 0;

    while (env.stage() == stage && n < limit)
    {
        env.process (p);
        ++n;
    }

    return n;
}
} // namespace

TEST_CASE ("Envelope: 10 ms attack reaches 1.0 at 10 ms, convex (0.675 at 5 ms)", "[dsp][env]")
{
    Envelope env;
    env.prepare (kRate);
    const EnvParams p { 0.010f, 1.0f, 0.5f, 1.0f };
    env.gateOn();

    float atHalf = 0.0f;

    for (int i = 0; i < 480; ++i) // 5 ms
        atHalf = env.process (p);

    REQUIRE (atHalf == Approx (0.675f).margin (0.02f));

    const int remaining = samplesInStage (env, p, Stage::Attack);
    const double attackMs = (480 + remaining) * 1000.0 / kRate;
    REQUIRE (attackMs == Approx (10.0).margin (0.2));
    REQUIRE (env.getLevel() == 1.0f);
    REQUIRE (env.stage() == Stage::Decay);
}

TEST_CASE ("Envelope: 1 s decay to sustain 0 is at 0.001 after 1 s", "[dsp][env]")
{
    Envelope env;
    env.prepare (kRate);
    const EnvParams p { 0.001f, 1.0f, 0.0f, 1.0f };
    env.gateOn();
    samplesInStage (env, p, Stage::Attack);

    int n = 0;

    while (env.getLevel() > 0.001f)
    {
        env.process (p);
        ++n;
    }

    REQUIRE (n / kRate == Approx (1.0).epsilon (0.02));
}

TEST_CASE ("Envelope: 0.5 s release: 0.001 at 0.5 s, idle at 0.83 s, monotonic", "[dsp][env]")
{
    Envelope env;
    env.prepare (kRate);
    const EnvParams p { 0.001f, 0.001f, 1.0f, 0.5f };
    env.gateOn();
    samplesInStage (env, p, Stage::Attack);
    samplesInStage (env, p, Stage::Decay);
    REQUIRE (env.getLevel() == 1.0f);

    env.gateOff();
    int n = 0, toThousandth = -1;
    float previous = env.getLevel();
    bool monotonic = true;

    while (! env.isIdle())
    {
        const float level = env.process (p);
        monotonic = monotonic && level <= previous;
        previous = level;
        ++n;

        if (toThousandth < 0 && level <= 0.001f)
            toThousandth = n;
    }

    REQUIRE (monotonic);
    REQUIRE (toThousandth / kRate == Approx (0.5).epsilon (0.02));
    REQUIRE (n / kRate == Approx (0.8333).epsilon (0.02));
}

TEST_CASE ("Envelope: retrigger during release continues from the current level", "[dsp][env]")
{
    Envelope env;
    env.prepare (kRate);
    const EnvParams p { 0.001f, 0.001f, 1.0f, 1.0f };
    env.gateOn();
    samplesInStage (env, p, Stage::Attack);
    env.gateOff();

    while (env.getLevel() > 0.3f)
        env.process (p);

    const float before = env.getLevel();
    env.gateOn();
    REQUIRE (env.process (p) >= before);
}

TEST_CASE ("Envelope: gate off during attack at 0.5 falls from there", "[dsp][env]")
{
    Envelope env;
    env.prepare (kRate);
    const EnvParams p { 1.0f, 1.0f, 1.0f, 1.0f };
    env.gateOn();

    while (env.getLevel() < 0.5f)
        env.process (p);

    const float at = env.getLevel();
    env.gateOff();
    const float next = env.process (p);
    const float after = env.process (p);

    REQUIRE (next <= at);
    REQUIRE (after < next);
}

TEST_CASE ("Envelope: sustain changes move smoothly with the Lin 20 smoother in front", "[dsp][env]")
{
    Envelope env;
    env.prepare (kRate);
    p5x::dsp::LinearSmoother sustain;
    sustain.prepare (kRate, 20.0);
    sustain.reset (0.2f);

    EnvParams p { 0.001f, 0.001f, 0.2f, 1.0f };
    env.gateOn();

    for (int i = 0; i < 20000; ++i)
        env.process (p);

    REQUIRE (env.stage() == Stage::Sustain);

    sustain.setTarget (1.0f);
    float previous = env.getLevel();

    for (int i = 0; i < 5000; ++i)
    {
        p.sustain = sustain.next();
        const float level = env.process (p);
        REQUIRE (std::abs (level - previous) <= 0.01f);
        previous = level;
    }

    REQUIRE (env.getLevel() == Approx (1.0f));
}

TEST_CASE ("Envelope: extreme times: no NaN, stages in order", "[dsp][env]")
{
    for (float t : { 0.001f, 15.0f })
    {
        Envelope env;
        env.prepare (kRate);
        const EnvParams p { std::min (t, 10.0f), t, 0.5f, t };
        env.gateOn();

        Stage last = env.stage();
        int transitions = 0;
        const int limit = (int) (kRate * 60.0);

        for (int i = 0; i < limit && ! env.isIdle(); ++i)
        {
            if (i == (int) (kRate * 40.0))
                env.gateOff();

            REQUIRE (std::isfinite (env.process (p)));

            if (env.stage() != last)
            {
                const bool forward = (last == Stage::Attack && env.stage() == Stage::Decay)
                                  || (last == Stage::Decay && env.stage() == Stage::Sustain)
                                  || (env.stage() == Stage::Release)
                                  || (last == Stage::Release && env.stage() == Stage::Idle);
                INFO ("time " << t << ": " << (int) last << " -> " << (int) env.stage());
                REQUIRE (forward);
                last = env.stage();
                ++transitions;
            }
        }

        REQUIRE (transitions >= 2);
    }
}

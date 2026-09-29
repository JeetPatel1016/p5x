// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#include "dsp/Voice.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

using Catch::Approx;
using p5x::dsp::Voice;

namespace
{
std::vector<float> render (Voice& v, int numSamples, float pitchOffset = 0.0f)
{
    std::vector<float> out ((size_t) numSamples, 0.0f);
    std::vector<float> offsets ((size_t) numSamples, pitchOffset);
    v.render (out.data(), offsets.data(), numSamples);
    return out;
}

double measureFrequency (const std::vector<float>& signal, double sampleRate)
{
    int crossings = 0, first = -1, last = -1;

    for (size_t i = 1; i < signal.size(); ++i)
    {
        if (signal[i - 1] < 0.0f && signal[i] >= 0.0f)
        {
            if (first < 0)
                first = (int) i;

            last = (int) i;
            ++crossings;
        }
    }

    return crossings > 1 ? (crossings - 1) * sampleRate / (last - first) : 0.0;
}
} // namespace

TEST_CASE ("Voice (M1 sine): idle voices are silent", "[dsp][voice]")
{
    Voice v;
    v.prepare (48000.0);
    const auto out = render (v, 256);

    for (float s : out)
        REQUIRE (s == 0.0f);

    REQUIRE (v.isIdle());
}

TEST_CASE ("Voice (M1 sine): 5 ms linear attack, then sustain", "[dsp][voice]")
{
    Voice v;
    v.prepare (48000.0);
    v.start (69, 1.0f);

    render (v, 239); // 5 ms = 240 samples
    REQUIRE (v.getStage() == Voice::Stage::Attack);
    REQUIRE (v.getLevel() == Approx (239.0 / 240.0).margin (1.0e-3));

    render (v, 2);
    REQUIRE (v.getStage() == Voice::Stage::Sustain);
    REQUIRE (v.getLevel() == 1.0f);
}

TEST_CASE ("Voice (M1 sine): 100 ms release to idle", "[dsp][voice]")
{
    Voice v;
    v.prepare (48000.0);
    v.start (60, 1.0f);
    render (v, 480);
    v.release();

    render (v, 4700);
    REQUIRE_FALSE (v.isIdle());

    render (v, 200); // 4800 samples = 100 ms in total
    REQUIRE (v.isIdle());
    REQUIRE (v.getLevel() == 0.0f);
}

TEST_CASE ("Voice (M1 sine): retrigger continues from the current level", "[dsp][voice]")
{
    Voice v;
    v.prepare (48000.0);
    v.start (60, 1.0f);
    render (v, 480);
    v.release();
    render (v, 2400); // halfway down

    const float before = v.getLevel();
    v.start (62, 1.0f);
    render (v, 1);

    REQUIRE (v.getLevel() >= before);
    REQUIRE (v.getStage() == Voice::Stage::Attack);
}

TEST_CASE ("Voice (M1 sine): pitch, bend offset and gain", "[dsp][voice]")
{
    constexpr double rate = 48000.0;
    Voice v;
    v.prepare (rate);
    v.start (69, 1.0f);
    render (v, 480); // past the attack

    const auto a440 = render (v, 48000);
    REQUIRE (measureFrequency (a440, rate) == Approx (440.0).epsilon (0.002));

    float peak = 0.0f;

    for (float s : a440)
        peak = std::max (peak, std::abs (s));

    REQUIRE (peak == Approx (Voice::kVoiceGain).margin (1.0e-3));

    const auto bentUp = render (v, 48000, 2.0f);
    REQUIRE (measureFrequency (bentUp, rate) == Approx (440.0 * std::pow (2.0, 2.0 / 12.0)).epsilon (0.002));
}

TEST_CASE ("Voice (M1 sine): extreme pitch offsets stay finite", "[dsp][voice]")
{
    Voice v;
    v.prepare (44100.0);
    v.start (96, 1.0f);

    for (float offset : { -1000.0f, 1000.0f, 0.0f })
    {
        for (float s : render (v, 1024, offset))
            REQUIRE (std::isfinite (s));
    }
}

TEST_CASE ("Voice (M1 sine): kill cuts immediately", "[dsp][voice]")
{
    Voice v;
    v.prepare (48000.0);
    v.start (60, 1.0f);
    render (v, 100);
    v.kill();

    REQUIRE (v.isIdle());

    for (float s : render (v, 64))
        REQUIRE (s == 0.0f);
}

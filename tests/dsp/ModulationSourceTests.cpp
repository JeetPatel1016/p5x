// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
// Global modulation sources (05-modulation.md): LFO, Wheel-Mod pink noise, the Wheel-Mod source mix.
#include "dsp/Lfo.h"
#include "dsp/PinkNoise.h"
#include "dsp/WheelModSource.h"
#include "support/Spectrum.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

using Catch::Approx;
using p5x::dsp::Lfo;
using p5x::dsp::PinkNoise;
using p5x::dsp::WheelModSource;

namespace
{
constexpr double kRate = 48000.0;
} // namespace

TEST_CASE ("LFO: 1 Hz has a period of 1 s +-0.5 %", "[dsp][lfo]")
{
    Lfo lfo;
    lfo.prepare (kRate, 0.25);

    // The saw's wraps (a big downward jump) mark whole periods.
    std::vector<int> wraps;
    float previous = lfo.process (1.0, Lfo::Shape::Saw);

    for (int i = 1; i < (int) (kRate * 5.5); ++i)
    {
        const float s = lfo.process (1.0, Lfo::Shape::Saw);

        if (s < previous - 1.0f)
            wraps.push_back (i);

        previous = s;
    }

    REQUIRE (wraps.size() >= 5);

    for (size_t k = 1; k < wraps.size(); ++k)
        REQUIRE ((double) (wraps[k] - wraps[k - 1]) == Approx (kRate).epsilon (0.005));
}

TEST_CASE ("LFO: shapes are bipolar and follow the formulas", "[dsp][lfo]")
{
    for (const auto shape : { Lfo::Shape::Saw, Lfo::Shape::Triangle, Lfo::Shape::Square })
    {
        Lfo lfo;
        lfo.prepare (kRate, 0.0);
        float lo = 1.0f, hi = -1.0f;

        for (int i = 0; i < (int) kRate; ++i)
        {
            const double phase = lfo.getPhase();
            const float s = lfo.process (4.0, shape);
            lo = std::min (lo, s);
            hi = std::max (hi, s);

            if (shape == Lfo::Shape::Saw)
                REQUIRE (s == Approx (2.0 * phase - 1.0).margin (1e-6));
            else if (shape == Lfo::Shape::Triangle)
                REQUIRE (s == Approx (1.0 - 4.0 * std::abs (phase - 0.5)).margin (1e-6));
        }

        REQUIRE (lo >= -1.0f);
        REQUIRE (hi <= 1.0f);
        REQUIRE (lo < -0.99f);
        REQUIRE (hi > 0.99f);
    }
}

TEST_CASE ("LFO: square transitions are slewed over 1 ms", "[dsp][lfo]")
{
    // No sample-to-sample jump above 2 / (0.001 × rate): a full swing spread over 1 ms (05 § Tests).
    for (const double rate : { 44100.0, 48000.0, 96000.0 })
    {
        Lfo lfo;
        lfo.prepare (rate, 0.1);
        const double bound = 2.0 / (0.001 * rate) + 1e-6;
        float previous = lfo.process (30.0, Lfo::Shape::Square);
        float lo = 1.0f, hi = -1.0f;

        for (int i = 0; i < (int) rate * 2; ++i)
        {
            const float s = lfo.process (30.0, Lfo::Shape::Square);
            REQUIRE (std::abs (s - previous) <= bound);
            lo = std::min (lo, s);
            hi = std::max (hi, s);
            previous = s;
        }

        REQUIRE (lo == -1.0f);
        REQUIRE (hi == 1.0f);
    }
}

TEST_CASE ("LFO: rate is clamped and the output stays finite", "[dsp][lfo]")
{
    Lfo lfo;
    lfo.prepare (kRate, 0.5);

    for (const double rate : { -5.0, 0.0, 1e9, std::nan (""), 1e-9 })
        for (int i = 0; i < 1000; ++i)
        {
            const float s = lfo.process (rate, Lfo::Shape::Triangle);
            REQUIRE (std::isfinite (s));
            REQUIRE (std::abs (s) <= 1.0f);
        }
}

TEST_CASE ("Pink noise: RMS about 0.35", "[dsp][pink]")
{
    PinkNoise pink;
    pink.prepare (1234);
    std::vector<float> x ((size_t) (kRate * 20.0));

    for (auto& s : x)
        s = pink.process();

    REQUIRE (p5x::test::rms (x) == Approx (PinkNoise::kTargetRms).epsilon (0.05));
}

TEST_CASE ("Pink noise: -3 dB per octave (equal energy per octave)", "[dsp][pink]")
{
    PinkNoise pink;
    pink.prepare (99);
    std::vector<float> x (1u << 20); // ~21.8 s at 48 kHz

    for (auto& s : x)
        s = pink.process();

    const auto mag = p5x::test::magnitudeSpectrum (x);
    const double binHz = kRate / (double) x.size();

    // Energy per octave band from 62.5 Hz to 8 kHz; pink noise puts the same energy in each.
    std::vector<double> bandDb;

    for (double lo = 62.5; lo < 8000.0; lo *= 2.0)
    {
        double energy = 0.0;

        for (auto k = (size_t) std::ceil (lo / binHz); (double) k < 2.0 * lo / binHz; ++k)
            energy += mag[k] * mag[k];

        bandDb.push_back (10.0 * std::log10 (energy));
    }

    double mean = 0.0;

    for (double db : bandDb)
        mean += db / (double) bandDb.size();

    for (double db : bandDb)
        REQUIRE (std::abs (db - mean) < 1.5);
}

TEST_CASE ("Pink noise: same seed repeats, different seed differs", "[dsp][pink]")
{
    PinkNoise a, b, c;
    a.prepare (5);
    b.prepare (5);
    c.prepare (6);
    bool differs = false;

    for (int i = 0; i < 1000; ++i)
    {
        const float s = a.process();
        REQUIRE (s == b.process());
        differs = differs || s != c.process();
    }

    REQUIRE (differs);
}

TEST_CASE ("Wheel-Mod source: LFO at mix 0, noise at mix 1, interpolated to the internal rate", "[dsp][wheelmod]")
{
    constexpr int factor = 2, n = 4800;
    std::vector<float> out ((size_t) (n * factor));

    WheelModSource lfoOnly;
    lfoOnly.prepare (kRate, factor, 7);
    lfoOnly.setParameters (5.0f, Lfo::Shape::Triangle, 0.0f);
    lfoOnly.resetSmoothing (5.0f, 0.0f);
    lfoOnly.render (out.data(), n);

    // Smooth triangle: interpolated steps are tiny and alternate samples sit between host samples.
    for (int i = 2 * factor; i < n * factor; ++i) // after the first host sample (ramps up from 0 at prepare)
        REQUIRE (std::abs (out[(size_t) i] - out[(size_t) i - 1]) <= 4.0f * 5.0f / (float) (kRate * factor) + 1e-5f);

    for (int j = 2; j < n; ++j)
    {
        const float mid = out[(size_t) (j * factor)], before = out[(size_t) (j * factor - 1)],
                    after = out[(size_t) (j * factor + 1)];
        REQUIRE (mid == Approx (0.5f * (before + after)).margin (1e-5)); // on the line between host samples
    }

    WheelModSource noiseOnly;
    noiseOnly.prepare (kRate, factor, 7);
    noiseOnly.setParameters (5.0f, Lfo::Shape::Triangle, 1.0f);
    noiseOnly.resetSmoothing (5.0f, 1.0f);
    std::vector<float> noise ((size_t) (kRate * 10.0 * factor));
    noiseOnly.render (noise.data(), (int) (kRate * 10.0));

    // Linear interpolation lowers the RMS a little; it stays near the pink noise level.
    REQUIRE (p5x::test::rms (noise) == Approx (PinkNoise::kTargetRms).epsilon (0.1));
}

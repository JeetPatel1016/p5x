// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
// 02-oscillators.md § Tests. The saw aliasing test runs through the real downsampler in the plugin
// tests (plugin/AliasingTests.cpp); Lo Freq is a voice feature (dsp/VoiceTests.cpp).
#include "dsp/Oscillator.h"
#include "dsp/Random.h"
#include "support/Spectrum.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using Catch::Approx;
using p5x::dsp::Oscillator;
using p5x::dsp::OscShapes;

namespace
{
constexpr double kRate = 96000.0;

std::vector<float> render (Oscillator& osc, double freq, float pw, OscShapes shapes, size_t n)
{
    std::vector<float> out (n);

    for (auto& s : out)
        s = osc.process (freq, pw, shapes);

    return out;
}
} // namespace

TEST_CASE ("Oscillator: residuals are the standard 2-sample polynomials", "[dsp][osc]")
{
    // BLEP: continuous at the step, zero outside ±1 sample; BLAMP is its integral.
    REQUIRE (Oscillator::blep (-1.0) == 0.0);
    REQUIRE (Oscillator::blep (-1.0e-9) == Approx (0.5).margin (1e-6));
    REQUIRE (Oscillator::blep (0.0) == Approx (-0.5));
    REQUIRE (Oscillator::blep (0.999999) == Approx (0.0).margin (1e-6));
    REQUIRE (Oscillator::blamp (0.0) == Approx (1.0 / 6.0));
    REQUIRE (Oscillator::blamp (-1.0e-9) == Approx (1.0 / 6.0).margin (1e-6));
    REQUIRE (Oscillator::blamp (1.0) == 0.0);
}

TEST_CASE ("Oscillator: saw pitch is correct", "[dsp][osc]")
{
    Oscillator osc;
    osc.prepare (kRate);
    osc.reset (0.0);
    const auto x = render (osc, 440.0, 0.5f, { true, false, false }, 65536);
    const auto mag = p5x::test::magnitudeSpectrum (x);

    size_t peak = 1;

    for (size_t k = 2; k < mag.size(); ++k)
        if (mag[k] > mag[peak])
            peak = k;

    REQUIRE ((double) peak * kRate / 65536.0 == Approx (440.0).margin (kRate / 65536.0));
}

TEST_CASE ("Oscillator: pulse at PW 0.5 has no DC; PW 0.05 and 0.95 are audible and bounded", "[dsp][osc]")
{
    Oscillator osc;
    osc.prepare (kRate);
    osc.reset (0.0);

    // 1 kHz divides 96 kHz exactly: whole periods, so the mean is the DC.
    auto x = render (osc, 1000.0, 0.5f, { false, false, true }, 96000);
    double mean = 0.0;

    for (float s : x)
        mean += s;

    REQUIRE (std::abs (mean / (double) x.size()) <= 0.01);

    for (float pw : { 0.05f, 0.95f })
    {
        x = render (osc, 1000.0, pw, { false, false, true }, 96000);
        float peak = 0.0f;

        for (float s : x)
            peak = std::max (peak, std::abs (s));

        INFO ("pw " << pw);
        REQUIRE (p5x::test::rms (x) > 0.1);
        REQUIRE (peak <= 1.1f);
    }
}

TEST_CASE ("Oscillator: triangle has odd harmonics only, falling 12 dB/oct", "[dsp][osc]")
{
    constexpr size_t n = 65536;
    constexpr double f = 750.0; // 750 Hz = exactly 512 bins, so harmonics land on bins
    Oscillator osc;
    osc.prepare (kRate);
    osc.reset (0.0);
    const auto x = render (osc, f, 0.5f, { false, true, false }, n);
    const auto mag = p5x::test::magnitudeSpectrum (x);
    const double binsPerHarmonic = f * (double) n / kRate;
    const double fundamental = p5x::test::peakAround (mag, binsPerHarmonic);

    for (int h = 3; h <= 9; h += 2)
    {
        const double level = p5x::test::toDb (p5x::test::peakAround (mag, h * binsPerHarmonic) / fundamental);
        INFO ("harmonic " << h);
        REQUIRE (level == Approx (-40.0 * std::log10 ((double) h)).margin (2.0)); // 1/n²
    }

    for (int h = 2; h <= 10; h += 2)
    {
        INFO ("even harmonic " << h);
        REQUIRE (p5x::test::toDb (p5x::test::peakAround (mag, h * binsPerHarmonic) / fundamental) < -60.0);
    }
}

TEST_CASE ("Oscillator: shapes sum; all off is silence", "[dsp][osc]")
{
    // Three oscillators from the same phase, run side by side (each carries its own residual into
    // the next sample, so they can't be forked from one per sample).
    Oscillator a, sawOnly, pulseOnly;

    for (auto* o : { &a, &sawOnly, &pulseOnly })
    {
        o->prepare (kRate);
        o->reset (0.3);
    }

    for (int i = 0; i < 1000; ++i)
    {
        const float sum = a.process (523.0, 0.4f, { true, false, true });
        const float saw = sawOnly.process (523.0, 0.4f, { true, false, false });
        const float pulse = pulseOnly.process (523.0, 0.4f, { false, false, true });
        REQUIRE (sum == Approx (saw + pulse).margin (1e-5));
    }

    for (float s : render (a, 440.0, 0.5f, {}, 256))
        REQUIRE (s == 0.0f);
}

TEST_CASE ("Oscillator: extreme inputs never produce NaN or leave [0, 1) phase", "[dsp][osc]")
{
    Oscillator osc;
    osc.prepare (kRate);
    osc.reset (0.0);
    p5x::Random random (5);

    for (int i = 0; i < 200000; ++i)
    {
        const double freq = (random.nextDouble() * 2.0 - 1.0) * 1.0e6;
        const auto pw = (float) (random.nextDouble() * 3.0 - 1.0);
        const float s = osc.process (freq, pw, { true, true, true });

        REQUIRE (std::isfinite (s));
        REQUIRE (osc.getPhase() >= 0.0);
        REQUIRE (osc.getPhase() < 1.0);
    }

    REQUIRE (std::isfinite (osc.process (std::nan (""), std::nanf (""), { true, true, true })));
}

TEST_CASE ("Oscillator: sync master reports where in the sample it wrapped", "[dsp][osc][sync]")
{
    Oscillator b;
    b.prepare (kRate);
    b.reset (0.95);

    // dt = 0.1: from phase 0.95 the wrap comes half-way through the step.
    b.process (0.1 * kRate, 0.5f, { true, false, false });
    REQUIRE (b.wrappedThisSample());
    REQUIRE (b.wrapFraction() == Approx (0.5));

    b.process (0.1 * kRate, 0.5f, { true, false, false });
    REQUIRE_FALSE (b.wrappedThisSample());

    // The master wraps even with every shape off.
    b.reset (0.99);
    b.process (0.1 * kRate, 0.5f, {});
    REQUIRE (b.wrappedThisSample());
    REQUIRE (b.wrapFraction() == Approx (0.1));
}

TEST_CASE ("Oscillator: hard sync locks A's period to B's", "[dsp][osc][sync]")
{
    // A at 3.3× B with sync on: A's output repeats with B's period (96 samples) ±1 (02 § Tests).
    constexpr double fB = 1000.0, fA = 3300.0;
    constexpr int period = (int) (kRate / fB);

    auto renderA = [] (bool sync)
    {
        Oscillator a, b;
        a.prepare (kRate);
        b.prepare (kRate);
        a.reset (0.37);
        b.reset (0.81);
        std::vector<float> out (9600);

        for (auto& s : out)
        {
            b.process (fB, 0.5f, { true, false, false });
            const double at = sync && b.wrappedThisSample() ? b.wrapFraction() : -1.0;
            s = a.process (fA, 0.5f, { true, false, true }, at);
        }

        return out;
    };

    // Lag (in 60..140 samples) at which the signal best matches itself.
    auto bestLag = [] (const std::vector<float>& x)
    {
        int best = 0;
        double bestError = 1e30;

        for (int lag = 60; lag <= 140; ++lag)
        {
            double error = 0.0;

            for (size_t i = 1000; i < 8000; ++i)
                error += std::abs (x[i] - x[i + (size_t) lag]);

            if (error < bestError)
            {
                bestError = error;
                best = lag;
            }
        }

        return std::pair { best, bestError / 7000.0 };
    };

    const auto [lag, error] = bestLag (renderA (true));
    REQUIRE (std::abs (lag - period) <= 1);
    REQUIRE (error < 0.02);

    // Without sync A keeps its own period: one B period later it's 0.3 of a cycle out.
    const auto free = renderA (false);
    double freeError = 0.0;

    for (size_t i = 1000; i < 8000; ++i)
        freeError += std::abs (free[i] - free[i + (size_t) period]) / 7000.0;

    REQUIRE (freeError > 0.2);
}

TEST_CASE ("Oscillator: sync resets are band-limited like natural wraps", "[dsp][osc][sync]")
{
    // A synced saw's reset from phase q to 0 is a step of −2q; with the residual split over two
    // samples no sample-to-sample jump exceeds what a band-limited step of that size allows.
    Oscillator a, b;
    a.prepare (kRate);
    b.prepare (kRate);
    a.reset (0.0);
    b.reset (0.0);
    float previous = 0.0f, largest = 0.0f;

    for (int i = 0; i < 20000; ++i)
    {
        b.process (220.0, 0.5f, {});
        const float s = a.process (220.0 * 2.71, 0.5f, { true, false, false }, b.wrappedThisSample() ? b.wrapFraction() : -1.0);

        if (i > 0)
            largest = std::max (largest, std::abs (s - previous));

        previous = s;
    }

    // A naive reset would jump by up to 2 in one sample; the 2-sample residual spreads it.
    REQUIRE (largest < 1.6f);
}

TEST_CASE ("Oscillator: sync stays finite with random frequencies and fractions", "[dsp][osc][sync]")
{
    Oscillator a;
    a.prepare (kRate);
    a.reset (0.2);
    p5x::Random random (11);

    for (int i = 0; i < 200000; ++i)
    {
        const double freq = (random.nextDouble() * 2.0 - 1.0) * 1.0e6;
        const double at = random.nextDouble() < 0.3 ? random.nextDouble() : -1.0;
        const float s = a.process (freq, (float) random.nextDouble(), { true, true, true }, at);

        REQUIRE (std::isfinite (s));
        REQUIRE (std::abs (s) < 6.0f);
        REQUIRE (a.getPhase() >= 0.0);
        REQUIRE (a.getPhase() < 1.0);
    }
}

// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
// 02-oscillators.md § Tests: saw aliasing through the real 2× downsampler. The threshold is what the
// specified model (2-sample PolyBLEP at 2×) measures, recorded in DECISIONS.md.
#include "dsp/Oscillator.h"
#include "support/Spectrum.h"

#include <juce_dsp/juce_dsp.h>

#include <catch2/catch_test_macros.hpp>

#include <cstdio>

namespace
{
constexpr double kHostRate = 48000.0;
constexpr double kInternalRate = 96000.0;
constexpr size_t kFftSize = 65536;

// Worst aliasing component (dB relative to the fundamental) below host Nyquist.
double worstAliasDb (double freq)
{
    juce::dsp::Oversampling<float> os (1, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
    constexpr int block = 512;
    os.initProcessing (block);

    p5x::dsp::Oscillator osc;
    osc.prepare (kInternalRate);
    osc.reset (0.0);

    std::vector<float> zeros (block, 0.0f), hostOut;
    const size_t settle = 4096;

    while (hostOut.size() < kFftSize + settle)
    {
        const float* z = zeros.data();
        auto high = os.processSamplesUp (juce::dsp::AudioBlock<const float> (&z, 1, block));

        for (size_t i = 0; i < high.getNumSamples(); ++i)
            high.setSample (0, (int) i, osc.process (freq, 0.5f, { true, false, false }));

        std::vector<float> out (block);
        float* o = out.data();
        juce::dsp::AudioBlock<float> outBlock (&o, 1, block);
        os.processSamplesDown (outBlock);
        hostOut.insert (hostOut.end(), out.begin(), out.end());
    }

    const std::vector<float> x (hostOut.begin() + (long) settle, hostOut.begin() + (long) (settle + kFftSize));
    const auto mag = p5x::test::magnitudeSpectrum (x);
    const double binHz = kHostRate / (double) kFftSize;
    const double fundamental = p5x::test::peakAround (mag, freq / binHz);

    std::vector<bool> harmonic (mag.size(), false);

    for (double h = freq; h < kHostRate * 0.5; h += freq)
    {
        const int centre = (int) std::lround (h / binHz);

        for (int k = std::max (0, centre - 8); k <= std::min ((int) mag.size() - 1, centre + 8); ++k)
            harmonic[(size_t) k] = true;
    }

    double worst = 0.0;

    // Skip DC and the top of the band where the downsampler's own transition sits (above 20 kHz).
    for (size_t k = 16; k < (size_t) (20000.0 / binHz); ++k)
        if (! harmonic[k])
            worst = std::max (worst, mag[k]);

    return p5x::test::toDb (worst / fundamental);
}
} // namespace

TEST_CASE ("Saw aliasing from 1 kHz to C8 through the 2x downsampler", "[plugin][osc][aliasing]")
{
    double overallWorst = -300.0;

    for (double f : { 1000.0, 1318.51, 1760.0, 2349.32, 3135.96, 4186.01 })
    {
        const double db = worstAliasDb (f);
        std::printf ("saw %8.2f Hz: worst alias %.1f dB\n", f, db);
        overallWorst = std::max (overallWorst, db);
    }

    // Threshold from the measured model: worst case −53.8 dB at C8 (DECISIONS.md, milestone 2).
    REQUIRE (overallWorst < -52.0);
}

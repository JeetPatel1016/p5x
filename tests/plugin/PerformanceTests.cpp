// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
// 11-testing.md § Test types 5: 5 voices held, full-saw patch, res 0.8, 2× OS at 48 kHz, block 128:
// processBlock ≤ 10 % of one core (reported), failing above 15 %. Enforced in Release builds only.
#include "plugin/TestProcessor.h"
#include "dsp/Envelope.h"
#include "dsp/LadderFilterCEM.h"
#include "dsp/Oscillator.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdio>

TEST_CASE ("Performance: 5 held full-saw voices at 48 kHz, block 128", "[plugin][performance]")
{
    constexpr double rate = 48000.0;
    constexpr int block = 128;

    P5XAudioProcessor p (false);
    p5x::test::setParam (p, "osc_a_saw", 1.0f);
    p5x::test::setParam (p, "osc_a_pulse", 1.0f);
    p5x::test::setParam (p, "osc_b_saw", 1.0f);
    p5x::test::setParam (p, "osc_b_tri", 1.0f);
    p5x::test::setParam (p, "osc_b_pulse", 1.0f);
    p5x::test::setParam (p, "mix_osc_b", 1.0f);
    p5x::test::setParam (p, "mix_noise", 1.0f);
    p5x::test::setParam (p, "flt_res", 0.8f);
    p.prepareToPlay (rate, block);

    juce::AudioBuffer<float> buffer (2, block);
    juce::MidiBuffer midi;

    for (int n : { 48, 55, 60, 64, 67 })
        midi.addEvent (juce::MidiMessage::noteOn (1, n, (juce::uint8) 100), 0);

    p.processBlock (buffer, midi);
    midi.clear();

    const int blocks = (int) (10.0 * rate / block); // 10 s of audio
    const auto start = juce::Time::getHighResolutionTicks();

    for (int i = 0; i < blocks; ++i)
        p.processBlock (buffer, midi);

    const double elapsed = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - start);
    const double percent = 100.0 * elapsed / (blocks * block / rate);
    std::printf ("performance: processBlock uses %.2f %% of one core\n", percent);

    REQUIRE (p.getTelemetry().activeVoices == 5);

#if defined(NDEBUG)
    REQUIRE (percent < 15.0);
#endif
}

//==================================================================================================
// CPU profile (milestone 4): where the time goes, and the cost of every voice count, HQ and unison.
// Reported only; the budget above is the one enforced.
namespace
{
double percentOfCore (int voices, int notes, bool hq, bool unison)
{
    constexpr double rate = 48000.0;
    constexpr int block = 128;

    P5XAudioProcessor p (false);
    p.setHqMode (hq);
    p5x::test::setParam (p, "osc_a_saw", 1.0f);
    p5x::test::setParam (p, "osc_a_pulse", 1.0f);
    p5x::test::setParam (p, "osc_b_saw", 1.0f);
    p5x::test::setParam (p, "osc_b_tri", 1.0f);
    p5x::test::setParam (p, "osc_b_pulse", 1.0f);
    p5x::test::setParam (p, "mix_noise", 1.0f);
    p5x::test::setParam (p, "flt_res", 0.8f);
    p5x::test::setParam (p, "perf_voices", voices == 5 ? 0.0f : voices == 8 ? 1.0f : 2.0f);
    p5x::test::setParam (p, "perf_unison", unison ? 1.0f : 0.0f);
    p.prepareToPlay (rate, block);

    juce::AudioBuffer<float> buffer (2, block);
    juce::MidiBuffer midi;

    for (int n = 0; n < notes; ++n)
        midi.addEvent (juce::MidiMessage::noteOn (1, 48 + n * 3, (juce::uint8) 100), 0);

    p.processBlock (buffer, midi);
    midi.clear();

    const int blocks = (int) (3.0 * rate / block);
    const auto start = juce::Time::getHighResolutionTicks();

    for (int i = 0; i < blocks; ++i)
        p.processBlock (buffer, midi);

    const double elapsed = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - start);
    return 100.0 * elapsed / (blocks * block / rate);
}

template <typename Fn>
double nanosecondsPerSample (int samples, Fn&& fn)
{
    const auto start = juce::Time::getHighResolutionTicks();

    for (int i = 0; i < samples; ++i)
        fn();

    return 1.0e9 * juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - start) / samples;
}
} // namespace

TEST_CASE ("Performance: CPU profile by component and configuration", "[plugin][performance][profile]")
{
    constexpr int n = 2'000'000;
    constexpr double internalRate = 96000.0;
    volatile float sink = 0.0f;

    p5x::dsp::Oscillator osc;
    osc.prepare (internalRate);
    double f = 100.0;
    const double oscNs = nanosecondsPerSample (n, [&] { sink = osc.process (f += 0.001, 0.4f, { true, true, true }); });

    p5x::dsp::LadderFilterCEM filter;
    filter.prepare (internalRate);
    float x = 0.0f;
    const double filterNs = nanosecondsPerSample (n, [&] { sink = filter.process (x = -x + 0.3f, 1500.0f, 0.8f); });

    p5x::dsp::Envelope env;
    env.prepare (internalRate);
    env.gateOn();
    const double envNs = nanosecondsPerSample (n, [&] { sink = env.process ({ 0.01f, 0.5f, 0.6f, 0.3f }); });

    std::printf ("profile: per internal sample: oscillator (3 shapes) %.1f ns, filter %.1f ns, envelope %.1f ns\n",
                 oscNs, filterNs, envNs);
    std::printf ("profile: a voice uses 2 oscillators + filter + 2 envelopes = %.1f ns/sample = %.2f %% of a core at 96 kHz\n",
                 2 * oscNs + filterNs + 2 * envNs, (2 * oscNs + filterNs + 2 * envNs) * internalRate * 1.0e-7);

    for (const bool hq : { false, true })
        for (const int voices : { 5, 8, 10 })
            std::printf ("profile: %2d voices held, %dx: %5.2f %% of one core\n", voices, hq ? 4 : 2,
                         percentOfCore (voices, voices, hq, false));

    std::printf ("profile: unison (5 voices, 1 note), 2x: %5.2f %% of one core\n", percentOfCore (5, 1, false, true));
    (void) sink;
    SUCCEED();
}

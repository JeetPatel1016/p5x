// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
// 11-testing.md § Test types 5: 5 voices held, full-saw patch, res 0.8, 2× OS at 48 kHz, block 128:
// processBlock ≤ 10 % of one core (reported), failing above 15 %. Enforced in Release builds only.
#include "plugin/TestProcessor.h"

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

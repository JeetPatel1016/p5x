// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
// Milestone 3 through the whole processor (05-modulation.md § Tests): mod wheel → Wheel-Mod,
// LFO → voices, sync and Lo Freq wiring, and the debug scope's samples.
#include "plugin/TestProcessor.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace p5x::test;
using Catch::Approx;
using juce::MidiMessage;

namespace
{
constexpr double kRate = 48000.0;

void loadSeed (P5XAudioProcessor& p, uint64_t seed)
{
    juce::MemoryBlock state;
    p.getStateInformation (state);
    auto xml = P5XAudioProcessor::getXmlFromBinary (state.getData(), (int) state.getSize());
    xml->getChildByName ("Seed")->setAttribute ("value", juce::String (seed));
    juce::AudioProcessor::copyXmlToBinary (*xml, state);
    p.setStateInformation (state.getData(), (int) state.getSize());
}

std::vector<float> renderNote (P5XAudioProcessor& p, int blocks, int blockSize)
{
    std::vector<float> out;

    for (int b = 0; b < blocks; ++b)
    {
        const auto buffer = b == 0 ? process (p, blockSize, { { MidiMessage::noteOn (1, 57, (juce::uint8) 100), 0 } })
                                   : process (p, blockSize);
        out.insert (out.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + blockSize);
    }

    return out;
}

const char* const kWheelDestinations[] = { "wm_dest_freq_a", "wm_dest_freq_b", "wm_dest_pw_a", "wm_dest_pw_b",
                                           "wm_dest_filter" };
} // namespace

TEST_CASE ("Modulation: Wheel-Mod with the wheel at 0 is bit-identical to Wheel-Mod off", "[plugin][mod]")
{
    auto renderWith = [] (bool destinations)
    {
        P5XAudioProcessor p (false);
        loadSeed (p, 42);
        setParam (p, "osc_a_pulse", 1.0f);
        setParam (p, "osc_b_pulse", 1.0f);
        setParam (p, "lfo_rate", 12.0f);
        setParam (p, "wm_mix", 0.5f); // LFO and noise both running

        for (auto* id : kWheelDestinations)
            setParam (p, id, destinations ? 1.0f : 0.0f);

        p.prepareToPlay (kRate, 256);
        return renderNote (p, 150, 256);
    };

    REQUIRE (renderWith (true) == renderWith (false));
}

TEST_CASE ("Modulation: Poly-Mod amounts at 0 are bit-identical to Poly-Mod off", "[plugin][mod]")
{
    auto renderWith = [] (bool destinations)
    {
        P5XAudioProcessor p (false);
        loadSeed (p, 42);
        setParam (p, "osc_b_tri", 1.0f);

        for (auto* id : { "pm_dest_freq_a", "pm_dest_pw_a", "pm_dest_filter" })
            setParam (p, id, destinations ? 1.0f : 0.0f);

        p.prepareToPlay (kRate, 256);
        return renderNote (p, 150, 256);
    };

    REQUIRE (renderWith (true) == renderWith (false));
}

TEST_CASE ("Modulation: wheel at 1, LFO triangle into Freq A swings +-12 semitones", "[plugin][mod]")
{
    P5XAudioProcessor p (false);
    setParam (p, "lfo_rate", 1.0f);
    setParam (p, "lfo_shape", 1.0f); // Triangle
    setParam (p, "wm_dest_freq_a", 1.0f);
    constexpr int block = 16; // telemetry is per block: small blocks sample the LFO finely
    p.prepareToPlay (kRate, block);

    process (p, block, { { MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0 },
                         { MidiMessage::controllerEvent (1, 1, 127), 0 } });

    double lowest = 1e9, highest = 0.0;

    for (int b = 0; b < (int) (2.5 * kRate / block); ++b)
    {
        process (p, block);

        if (b * block < (int) (0.1 * kRate)) // let the 20 ms wheel smoothing settle
            continue;

        for (const auto& v : p.getTelemetry().voices)
            if (v.state != p5x::debug::VoiceTelemetry::State::Idle)
            {
                lowest = std::min (lowest, (double) v.oscAHz);
                highest = std::max (highest, (double) v.oscAHz);
            }
    }

    const double c4 = 261.6256;
    REQUIRE (12.0 * std::log2 (highest / c4) == Approx (12.0).margin (0.1));
    REQUIRE (12.0 * std::log2 (lowest / c4) == Approx (-12.0).margin (0.1));
}

TEST_CASE ("Modulation: Reset All Controllers returns the wheel to 0", "[plugin][mod]")
{
    P5XAudioProcessor p (false);
    setParam (p, "wm_dest_freq_a", 1.0f);
    loadSeed (p, 5);
    setParam (p, "lfo_rate", 0.05f); // square: at ±1 except during its 1 ms slews, 10 s apart
    setParam (p, "lfo_shape", 2.0f);
    p.prepareToPlay (kRate, 480);

    process (p, 480, { { MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0 },
                       { MidiMessage::controllerEvent (1, 1, 127), 0 } });

    for (int b = 0; b < 10; ++b)
        process (p, 480);

    auto oscAHz = [&p]
    {
        for (const auto& v : p.getTelemetry().voices)
            if (v.state != p5x::debug::VoiceTelemetry::State::Idle)
                return (double) v.oscAHz;

        return 0.0;
    };

    REQUIRE (std::abs (12.0 * std::log2 (oscAHz() / 261.6256)) == Approx (12.0).margin (0.01));

    process (p, 480, { { MidiMessage::controllerEvent (1, 121, 0), 0 } });

    for (int b = 0; b < 5; ++b)
        process (p, 480);

    REQUIRE (oscAHz() == Approx (261.6256).epsilon (1e-4));
}

TEST_CASE ("Modulation: sync and Lo Freq parameters reach the voices", "[plugin][mod]")
{
    auto renderWith = [] (const char* id, float value)
    {
        P5XAudioProcessor p (false);
        loadSeed (p, 3);
        setParam (p, "mix_osc_b", 1.0f);
        setParam (p, "osc_a_freq", 7.0f);
        setParam (p, id, value);
        p.prepareToPlay (kRate, 256);
        return renderNote (p, 40, 256);
    };

    REQUIRE (renderWith ("osc_a_sync", 1.0f) != renderWith ("osc_a_sync", 0.0f));
    REQUIRE (renderWith ("osc_b_lofreq", 1.0f) != renderWith ("osc_b_lofreq", 0.0f));
}

TEST_CASE ("Scope: telemetry carries the last 512 output samples, oldest first", "[plugin][scope]")
{
    P5XAudioProcessor p (false);
    p.prepareToPlay (kRate, 480);
    std::vector<float> out;

    for (int b = 0; b < 4; ++b)
    {
        const auto buffer = b == 0 ? process (p, 480, { { MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0 } })
                                   : process (p, 480);
        out.insert (out.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + 480);
    }

    const auto t = p.getTelemetry();
    const size_t offset = out.size() - (size_t) p5x::debug::kScopeSamples;

    for (size_t i = 0; i < (size_t) p5x::debug::kScopeSamples; ++i)
        REQUIRE (t.scope[i] == out[offset + i]);
}

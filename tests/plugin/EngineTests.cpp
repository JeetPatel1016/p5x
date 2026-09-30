// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
// Milestone 2 engine: oversampling latency, determinism, telemetry, Standalone input routing.
#include "plugin/TestProcessor.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace p5x::test;
using Catch::Approx;
using juce::MidiMessage;

namespace
{
constexpr double kRate = 48000.0;
constexpr int kBlock = 480;

std::vector<float> renderSeconds (P5XAudioProcessor& p, double seconds, std::initializer_list<TimedMessage> firstEvents = {})
{
    std::vector<float> out;
    const int blocks = (int) (seconds * kRate / kBlock);

    for (int b = 0; b < blocks; ++b)
    {
        const auto buffer = b == 0 ? process (p, kBlock, firstEvents) : process (p, kBlock);
        out.insert (out.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + kBlock);
    }

    return out;
}

void loadSeed (P5XAudioProcessor& p, uint64_t seed)
{
    juce::MemoryBlock state;
    p.getStateInformation (state);
    auto xml = P5XAudioProcessor::getXmlFromBinary (state.getData(), (int) state.getSize());
    xml->getChildByName ("Seed")->setAttribute ("value", juce::String (seed));
    juce::AudioProcessor::copyXmlToBinary (*xml, state);
    p.setStateInformation (state.getData(), (int) state.getSize());
}
} // namespace

TEST_CASE ("Engine: renders at 2x and reports the downsampler latency", "[plugin][engine]")
{
    P5XAudioProcessor p;
    p.prepareToPlay (kRate, kBlock);

    REQUIRE (P5XAudioProcessor::kOversamplingFactor == 2);
    REQUIRE (p.getLatencySamples() >= 1);
    REQUIRE (p.getLatencySamples() <= 16);

    process (p, kBlock, { { MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0 } });
    const auto t = p.getTelemetry();
    REQUIRE (t.oversampling == 2);
    REQUIRE (t.sampleRate == kRate);
}

TEST_CASE ("Engine: same seed renders identically; a different seed differs", "[plugin][engine]")
{
    auto renderWith = [] (uint64_t seed)
    {
        P5XAudioProcessor p;
        loadSeed (p, seed);
        setParam (p, "mix_noise", 0.5f);
        p.prepareToPlay (kRate, kBlock);
        return renderSeconds (p, 0.5, { { MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0 },
                                        { MidiMessage::noteOn (1, 67, (juce::uint8) 100), 0 } });
    };

    REQUIRE (renderWith (1) == renderWith (1));
    REQUIRE (renderWith (1) != renderWith (2));
}

TEST_CASE ("Engine: voice telemetry for the Voices table", "[plugin][engine]")
{
    P5XAudioProcessor p;
    p.prepareToPlay (kRate, kBlock);
    setParam (p, "flt_env_amt", 0.0f);
    process (p, kBlock, { { MidiMessage::noteOn (1, 69, (juce::uint8) 100), 0 } });
    process (p, kBlock);

    const auto t = p.getTelemetry();
    REQUIRE (t.activeVoices == 1);

    const auto& v = t.voices[0];
    REQUIRE (v.state == p5x::debug::VoiceTelemetry::State::On);
    REQUIRE (v.note == 69);
    REQUIRE (v.oscAHz == Approx (440.0f).epsilon (0.001));
    REQUIRE (v.cutoffHz == Approx (4000.0f).epsilon (0.01));
    REQUIRE (v.ampLevel > 0.9f);

    process (p, kBlock, { { MidiMessage::noteOff (1, 69), 0 } });
    REQUIRE (p.getTelemetry().voices[0].state == p5x::debug::VoiceTelemetry::State::Release);
}

TEST_CASE ("Engine: Standalone input routes into the voices only when switched on", "[plugin][engine]")
{
    P5XAudioProcessor p (true); // Standalone layout: stereo in, stereo out
    REQUIRE (p.hasInputBus());
    REQUIRE (p.getTotalNumInputChannels() == 2);

    // Oscillators silent from the start (set before prepare, so no smoother glide leaves a tail):
    // anything at the output came in through the input.
    setParam (p, "mix_osc_a", 0.0f);
    setParam (p, "mix_osc_b", 0.0f);
    setParam (p, "flt_cutoff", 20000.0f);
    p.prepareToPlay (kRate, kBlock);
    process (p, kBlock, { { MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0 } });

    auto runWithInput = [&p]
    {
        float peakOut = 0.0f;
        double phase = 0.0;

        for (int b = 0; b < 100; ++b)
        {
            juce::AudioBuffer<float> buffer (2, kBlock);

            for (int i = 0; i < kBlock; ++i)
            {
                const auto s = (float) (0.5 * std::sin (phase));
                buffer.setSample (0, i, s);
                buffer.setSample (1, i, s);
                phase += 2.0 * juce::MathConstants<double>::pi * 440.0 / kRate;
            }

            juce::MidiBuffer none;
            p.processBlock (buffer, none);

            if (b > 10)
                peakOut = std::max (peakOut, buffer.getMagnitude (0, 0, kBlock));
        }

        return peakOut;
    };

    REQUIRE_FALSE (p.isRoutingInput());
    REQUIRE (runWithInput() < 1.0e-5f); // off by default: the input never reaches the output

    p.setRouteInput (true);
    REQUIRE (runWithInput() > 0.01f);

    p.setRouteInput (false);
    process (p, kBlock * 10);
    REQUIRE (runWithInput() < 1.0e-4f);
}

TEST_CASE ("Engine: the VST3 layout has no input and rejects one", "[plugin][engine]")
{
    P5XAudioProcessor p (false);
    REQUIRE_FALSE (p.hasInputBus());
    REQUIRE (p.getTotalNumInputChannels() == 0);

    p.setRouteInput (true);
    REQUIRE_FALSE (p.isRoutingInput());
}

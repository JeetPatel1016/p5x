// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
// Milestone 4 through the whole processor (05-modulation.md, 06-voices.md § Tests): unison, glide,
// velocity, aftertouch, Vintage and HQ 4× oversampling.
#include "plugin/TestProcessor.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <set>

using namespace p5x::test;
using Catch::Approx;
using juce::MidiMessage;
using p5x::debug::VoiceTelemetry;

namespace
{
constexpr double kRate = 48000.0;
constexpr int kBlock = 480;

void loadSeed (P5XAudioProcessor& p, uint64_t seed)
{
    juce::MemoryBlock state;
    p.getStateInformation (state);
    auto xml = P5XAudioProcessor::getXmlFromBinary (state.getData(), (int) state.getSize());
    xml->getChildByName ("Seed")->setAttribute ("value", juce::String (seed));
    juce::AudioProcessor::copyXmlToBinary (*xml, state);
    p.setStateInformation (state.getData(), (int) state.getSize());
}

std::vector<float> renderBlocks (P5XAudioProcessor& p, int blocks, std::initializer_list<TimedMessage> first = {})
{
    std::vector<float> out;

    for (int b = 0; b < blocks; ++b)
    {
        const auto buffer = b == 0 ? process (p, kBlock, first) : process (p, kBlock);
        out.insert (out.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + kBlock);
    }

    return out;
}

// Notes of the sounding (non-idle, non-releasing) voices, from telemetry.
std::multiset<int> soundingNotes (P5XAudioProcessor& p)
{
    std::multiset<int> notes;

    for (const auto& v : p.getTelemetry().voices)
        if (v.state == VoiceTelemetry::State::On || v.state == VoiceTelemetry::State::Sustained)
            notes.insert (v.note);

    return notes;
}

P5XAudioProcessor& unisonSynth (P5XAudioProcessor& p)
{
    loadSeed (p, 1);
    setParam (p, "perf_unison", 1.0f);
    setParam (p, "perf_vintage", 0.0f);
    p.prepareToPlay (kRate, kBlock);
    return p;
}

MidiMessage on (int n) { return MidiMessage::noteOn (1, n, (juce::uint8) 100); }
MidiMessage off (int n) { return MidiMessage::noteOff (1, n); }
MidiMessage pedal (bool down) { return MidiMessage::controllerEvent (1, 64, down ? 127 : 0); }
} // namespace

TEST_CASE ("Unison: every active voice plays the one note, detuned +-7 cents", "[plugin][m4][unison]")
{
    P5XAudioProcessor p (false);
    unisonSynth (p);
    setParam (p, "flt_env_amt", 0.0f);
    renderBlocks (p, 4, { { on (69), 0 } });

    const auto t = p.getTelemetry();
    REQUIRE (soundingNotes (p) == std::multiset<int> { 69, 69, 69, 69, 69 });

    // Voice i of 5: (i/4 − 0.5) × 14 cents → −7, −3.5, 0, +3.5, +7.
    for (int i = 0; i < 5; ++i)
    {
        const double cents = 1200.0 * std::log2 (t.voices[(size_t) i].oscAHz / 440.0);
        REQUIRE (cents == Approx ((i / 4.0 - 0.5) * 14.0).margin (0.01));
    }
}

TEST_CASE ("Unison: releasing the top note returns to the previous held note", "[plugin][m4][unison]")
{
    P5XAudioProcessor p (false);
    unisonSynth (p);
    renderBlocks (p, 2, { { on (48), 0 } });
    renderBlocks (p, 2, { { on (52), 0 } });
    renderBlocks (p, 2, { { on (55), 0 } });
    REQUIRE (*soundingNotes (p).begin() == 55);

    renderBlocks (p, 2, { { off (55), 0 } });
    REQUIRE (soundingNotes (p) == std::multiset<int> { 52, 52, 52, 52, 52 });

    renderBlocks (p, 2, { { off (48), 0 } }); // a lower, non-current note: no change
    REQUIRE (*soundingNotes (p).begin() == 52);

    renderBlocks (p, 2, { { off (52), 0 } }); // nothing left: all voices release
    REQUIRE (soundingNotes (p).empty());
}

TEST_CASE ("Unison: the sustain pedal keeps released keys in the stack", "[plugin][m4][unison]")
{
    // DECISIONS.md 2026-10-23 (OPEN_QUESTIONS #30).
    P5XAudioProcessor p (false);
    unisonSynth (p);
    renderBlocks (p, 2, { { on (48), 0 } });
    renderBlocks (p, 2, { { pedal (true), 0 }, { on (55), 1 } });

    renderBlocks (p, 2, { { off (55), 0 } }); // key up, pedal down: 55 keeps sounding
    REQUIRE (*soundingNotes (p).begin() == 55);

    renderBlocks (p, 2, { { pedal (false), 0 } }); // pedal up: back to the held 48
    REQUIRE (soundingNotes (p) == std::multiset<int> { 48, 48, 48, 48, 48 });

    renderBlocks (p, 2, { { pedal (true), 0 }, { off (48), 1 } });
    REQUIRE (*soundingNotes (p).begin() == 48); // held by the pedal
    renderBlocks (p, 2, { { pedal (false), 0 } });
    REQUIRE (soundingNotes (p).empty());
}

TEST_CASE ("Unison: switching it on or off releases every voice", "[plugin][m4][unison]")
{
    P5XAudioProcessor p (false);
    loadSeed (p, 1);
    p.prepareToPlay (kRate, kBlock);
    renderBlocks (p, 2, { { on (60), 0 }, { on (64), 0 } });
    REQUIRE (soundingNotes (p).size() == 2);

    setParam (p, "perf_unison", 1.0f);
    renderBlocks (p, 1);
    REQUIRE (soundingNotes (p).empty());

    renderBlocks (p, 2, { { on (67), 0 } });
    REQUIRE (soundingNotes (p).size() == 5);

    setParam (p, "perf_unison", 0.0f);
    renderBlocks (p, 1);
    REQUIRE (soundingNotes (p).empty());
}

TEST_CASE ("Unison: notes glide together from the previous unison note", "[plugin][m4][unison]")
{
    P5XAudioProcessor p (false);
    unisonSynth (p);
    setParam (p, "perf_glide", 0.5f);
    renderBlocks (p, 2, { { on (48), 0 } }); // first unison note: no glide
    renderBlocks (p, 2, { { off (48), 0 } });

    renderBlocks (p, 25, { { on (60), 0 } }); // 0.25 s into a 0.5 s glide: F#3 on every voice
    const auto t = p.getTelemetry();

    for (int i = 0; i < 5; ++i)
    {
        const double note = 69.0 + 12.0 * std::log2 (t.voices[(size_t) i].oscAHz / 440.0);
        const double unisonCents = (i / 4.0 - 0.5) * 14.0;
        REQUIRE (note - unisonCents / 100.0 == Approx (54.0).margin (0.1));
    }
}

TEST_CASE ("Velocity off: notes at velocity 10 and 127 render identically", "[plugin][m4]")
{
    auto renderVelocity = [] (int velocity)
    {
        P5XAudioProcessor p (false);
        loadSeed (p, 2);
        setParam (p, "perf_velocity", 0.0f);
        p.prepareToPlay (kRate, kBlock);
        return renderBlocks (p, 50, { { MidiMessage::noteOn (1, 60, (juce::uint8) velocity), 0 } });
    };

    REQUIRE (renderVelocity (10) == renderVelocity (127));
}

TEST_CASE ("Aftertouch off: pressure messages don't change the output", "[plugin][m4]")
{
    auto renderWith = [] (bool pressure)
    {
        P5XAudioProcessor p (false);
        loadSeed (p, 2);
        setParam (p, "perf_aftertouch", 0.0f);
        setParam (p, "wm_dest_freq_a", 1.0f);
        setParam (p, "wm_dest_filter", 1.0f);
        p.prepareToPlay (kRate, kBlock);

        if (! pressure)
            return renderBlocks (p, 50, { { on (60), 0 } });

        return renderBlocks (p, 50, { { on (60), 0 },
                                      { MidiMessage::channelPressureChange (1, 127), 10 },
                                      { MidiMessage::aftertouchChange (1, 60, 127), 20 } });
    };

    REQUIRE (renderWith (true) == renderWith (false));
}

TEST_CASE ("Aftertouch on: channel and poly pressure drive Wheel-Mod", "[plugin][m4]")
{
    P5XAudioProcessor p (false);
    loadSeed (p, 2);
    setParam (p, "perf_vintage", 0.0f);
    setParam (p, "wm_dest_freq_a", 1.0f);
    setParam (p, "lfo_shape", 2.0f); // square
    setParam (p, "lfo_rate", 0.05f); // stays at one level for 10 s
    p.prepareToPlay (kRate, kBlock);

    auto semitones = [&p]
    {
        for (const auto& v : p.getTelemetry().voices)
            if (v.state == VoiceTelemetry::State::On)
                return std::abs (12.0 * std::log2 (v.oscAHz / 261.6256));

        return -1.0;
    };

    renderBlocks (p, 5, { { on (60), 0 } });
    REQUIRE (semitones() == Approx (0.0).margin (1e-3));

    renderBlocks (p, 5, { { MidiMessage::channelPressureChange (1, 127), 0 } });
    REQUIRE (semitones() == Approx (12.0).margin (0.01));

    renderBlocks (p, 5, { { MidiMessage::channelPressureChange (1, 0), 0 },
                          { MidiMessage::aftertouchChange (1, 60, 64), 0 } });
    REQUIRE (semitones() == Approx (12.0 * 64.0 / 127.0).margin (0.01));
}

TEST_CASE ("Vintage: same seed renders identically, a different seed differs", "[plugin][m4]")
{
    auto renderWithSeed = [] (uint64_t seed)
    {
        P5XAudioProcessor p (false);
        loadSeed (p, seed);
        setParam (p, "perf_vintage", 1.0f);
        p.prepareToPlay (kRate, kBlock);
        return renderBlocks (p, 50, { { on (60), 0 }, { on (64), 0 } });
    };

    REQUIRE (renderWithSeed (11) == renderWithSeed (11));
    REQUIRE (renderWithSeed (11) != renderWithSeed (12));
}

TEST_CASE ("HQ mode: 4x oversampling from the next prepareToPlay", "[plugin][m4][hq]")
{
    P5XAudioProcessor p (false);
    p.prepareToPlay (kRate, kBlock);
    REQUIRE (p.getOversamplingFactor() == 2);
    const int normalLatency = p.getLatencySamples();

    p.setHqMode (true);
    REQUIRE (p.getOversamplingFactor() == 2); // applies at the next prepare (06 § Oversampling modes)

    p.prepareToPlay (kRate, kBlock);
    REQUIRE (p.getOversamplingFactor() == 4);
    REQUIRE (p.getLatencySamples() >= 1);
    REQUIRE (p.getLatencySamples() <= 32);
    INFO ("latency 2x " << normalLatency << ", 4x " << p.getLatencySamples());

    renderBlocks (p, 5, { { on (60), 0 } });
    const auto t = p.getTelemetry();
    REQUIRE (t.oversampling == 4);
    REQUIRE (t.activeVoices == 1);
    REQUIRE (t.voices[0].oscAHz > 0.0f);
}

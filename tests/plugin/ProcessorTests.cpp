// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#include "AppPaths.h"
#include "plugin/TestProcessor.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace p5x::test;
using Catch::Approx;
using juce::MidiMessage;

namespace
{
constexpr double kRate = 48000.0;
constexpr int kBlock = 512;

struct Prepared
{
    Prepared()
    {
        p5x::paths::settingsFile().deleteFile();
        p5x::paths::midiMapFile().deleteFile();
        processor.prepareToPlay (kRate, kBlock);
    }

    // Renders `seconds` of audio in kBlock chunks, returning the last block.
    juce::AudioBuffer<float> run (double seconds)
    {
        juce::AudioBuffer<float> last;
        const int blocks = juce::jmax (1, (int) std::ceil (seconds * kRate / kBlock));

        for (int i = 0; i < blocks; ++i)
            last = process (processor, kBlock);

        return last;
    }

    P5XAudioProcessor processor;
    uint64_t logPosition = 0;
};

// Rate-limited log call sites share their per-second budget across tests; start a fresh second.
void waitForFreshRateWindow()
{
    juce::Thread::sleep (1100);
}
} // namespace

TEST_CASE ("Processor: sine per note, both channels, correct pitch", "[plugin][processor]")
{
    Prepared f;
    process (f.processor, kBlock, { { MidiMessage::noteOn (1, 69, (juce::uint8) 100), 0 } });

    juce::AudioBuffer<float> out (2, 48000);
    juce::MidiBuffer none;
    f.processor.processBlock (out, none);

    REQUIRE (peak (out) == Approx (0.16f).margin (0.01));
    REQUIRE (frequency (out, kRate) == Approx (440.0).epsilon (0.002));

    for (int i = 0; i < out.getNumSamples(); ++i)
        REQUIRE (out.getSample (0, i) == out.getSample (1, i));
}

TEST_CASE ("Processor: MIDI is sample-accurate within the block", "[plugin][processor]")
{
    Prepared f;
    const auto out = process (f.processor, kBlock, { { MidiMessage::noteOn (1, 60, (juce::uint8) 100), 300 } });

    for (int i = 0; i < 300; ++i)
        REQUIRE (out.getSample (0, i) == 0.0f);

    REQUIRE (peak (out, 300) > 0.0f);
}

TEST_CASE ("Processor: 5 voices, 6 held notes: the 6th steals and logs a WARN", "[plugin][processor]")
{
    Prepared f;
    waitForFreshRateWindow();
    logLines (f.processor, f.logPosition);

    for (int n = 60; n < 66; ++n)
        process (f.processor, kBlock, { { MidiMessage::noteOn (1, n, (juce::uint8) 100), 0 } });

    const auto lines = logLines (f.processor, f.logPosition);
    REQUIRE (containsLine (lines, "WARN Voice"));
    REQUIRE (containsLine (lines, "stolen: note 60 -> 65"));
    REQUIRE (f.processor.getTelemetry().activeVoices == 5);
}

TEST_CASE ("Processor: Panic silences everything within one block", "[plugin][processor]")
{
    Prepared f;
    process (f.processor, kBlock, { { MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0 },
                                     { MidiMessage::noteOn (1, 64, (juce::uint8) 100), 0 },
                                     { MidiMessage::controllerEvent (1, 64, 127), 0 } });
    f.run (0.1);

    f.processor.panic();
    REQUIRE (peak (process (f.processor, kBlock)) == 0.0f);
    REQUIRE (peak (f.run (0.2)) == 0.0f);
}

TEST_CASE ("Processor: All Sound Off cuts, All Notes Off releases", "[plugin][processor]")
{
    Prepared f;
    process (f.processor, kBlock, { { MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0 } });
    const auto cut = process (f.processor, kBlock, { { MidiMessage::allSoundOff (1), 100 } });
    REQUIRE (peak (cut, 100) == 0.0f);

    process (f.processor, kBlock, { { MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0 },
                                     { MidiMessage::controllerEvent (1, 64, 127), 0 },
                                     { MidiMessage::noteOff (1, 60), 10 } });
    REQUIRE (peak (f.run (0.3)) > 0.0f); // held by the pedal

    process (f.processor, kBlock, { { MidiMessage::allNotesOff (1), 0 } });
    REQUIRE (peak (f.run (0.2)) == 0.0f); // released after 100 ms; sustain state cleared
}

TEST_CASE ("Processor: sustain pedal holds notes until pedal up", "[plugin][processor]")
{
    Prepared f;
    process (f.processor, kBlock, { { MidiMessage::controllerEvent (1, 64, 127), 0 },
                                     { MidiMessage::noteOn (1, 60, (juce::uint8) 100), 1 },
                                     { MidiMessage::noteOff (1, 60), 200 } });
    REQUIRE (peak (f.run (0.5)) > 0.0f);

    process (f.processor, kBlock, { { MidiMessage::controllerEvent (1, 64, 0), 0 } });
    REQUIRE (peak (f.run (0.2)) == 0.0f);
}

TEST_CASE ("Processor: channel filter 3 ignores channel 1", "[plugin][processor]")
{
    Prepared f;
    f.processor.setMidiChannel (3);

    process (f.processor, kBlock, { { MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0 } });
    REQUIRE (peak (f.run (0.05)) == 0.0f);

    process (f.processor, kBlock, { { MidiMessage::noteOn (3, 60, (juce::uint8) 100), 0 } });
    REQUIRE (peak (f.run (0.05)) > 0.0f);
}

TEST_CASE ("Processor: notes outside 36-96 are ignored", "[plugin][processor]")
{
    Prepared f;
    process (f.processor, kBlock, { { MidiMessage::noteOn (1, 35, (juce::uint8) 100), 0 },
                                     { MidiMessage::noteOn (1, 97, (juce::uint8) 100), 0 } });
    REQUIRE (peak (f.run (0.05)) == 0.0f);

    process (f.processor, kBlock, { { MidiMessage::noteOn (1, 36, (juce::uint8) 100), 0 } });
    REQUIRE (peak (f.run (0.05)) > 0.0f);
}

TEST_CASE ("Processor: pitch bend follows bend_range", "[plugin][processor]")
{
    Prepared f;
    setParam (f.processor, "bend_range", 2.0f);
    process (f.processor, kBlock, { { MidiMessage::noteOn (1, 69, (juce::uint8) 100), 0 },
                                     { MidiMessage::pitchWheel (1, 16383), 0 } });
    f.run (0.1);

    juce::AudioBuffer<float> out (2, 48000);
    juce::MidiBuffer none;
    f.processor.processBlock (out, none);
    REQUIRE (frequency (out, kRate) == Approx (440.0 * std::pow (2.0, 2.0 / 12.0)).epsilon (0.003));

    // Reset All Controllers centres the bend.
    process (f.processor, kBlock, { { MidiMessage::controllerEvent (1, 121, 0), 0 } });
    f.run (0.05);
    f.processor.processBlock (out, none);
    REQUIRE (frequency (out, kRate) == Approx (440.0).epsilon (0.003));
}

TEST_CASE ("Processor: master tune and volume", "[plugin][processor]")
{
    Prepared f;
    setParam (f.processor, "master_tune", 100.0f); // +1 semitone
    process (f.processor, kBlock, { { MidiMessage::noteOn (1, 69, (juce::uint8) 100), 0 } });
    f.run (0.1);

    juce::AudioBuffer<float> out (2, 48000);
    juce::MidiBuffer none;
    f.processor.processBlock (out, none);
    REQUIRE (frequency (out, kRate) == Approx (440.0 * std::pow (2.0, 1.0 / 12.0)).epsilon (0.003));

    setParam (f.processor, "master_volume", -60.0f);
    f.run (0.05);
    REQUIRE (peak (process (f.processor, kBlock)) == 0.0f);
}

TEST_CASE ("Processor: Voices = 8 applies when idle", "[plugin][processor]")
{
    Prepared f;
    waitForFreshRateWindow();
    setParam (f.processor, "perf_voices", 1.0f); // "8"
    process (f.processor, kBlock);
    REQUIRE (f.processor.getTelemetry().voiceCount == 8);

    logLines (f.processor, f.logPosition);

    for (int n = 60; n < 68; ++n)
        process (f.processor, kBlock, { { MidiMessage::noteOn (1, n, (juce::uint8) 100), 0 } });

    REQUIRE_FALSE (containsLine (logLines (f.processor, f.logPosition), "stolen"));

    process (f.processor, kBlock, { { MidiMessage::noteOn (1, 70, (juce::uint8) 100), 0 } });
    REQUIRE (containsLine (logLines (f.processor, f.logPosition), "stolen"));
}

TEST_CASE ("Processor: MIDI Learn end to end through processBlock", "[plugin][processor][learn]")
{
    Prepared f;
    waitForFreshRateWindow();
    auto& learn = f.processor.getMidiLearn();
    const int cutoff = f.processor.getParameterIndex ("flt_cutoff");
    f.processor.setTakeover (p5x::midi::Takeover::Jump);
    logLines (f.processor, f.logPosition);

    learn.startLearn (cutoff);
    process (f.processor, kBlock, { { MidiMessage::controllerEvent (1, 64, 127), 0 } });
    process (f.processor, kBlock, { { MidiMessage::controllerEvent (1, 74, 10), 0 } });
    f.processor.serviceMessageThread();

    auto lines = logLines (f.processor, f.logPosition);
    REQUIRE (containsLine (lines, "WARN CC 64 is reserved (sustain)"));
    REQUIRE (containsLine (lines, "INFO Filter Cutoff <- CC 74"));
    REQUIRE (learn.getCcForParam (cutoff) == 74);

    // Sound path is immediate (override); the host path lands on the message thread.
    process (f.processor, kBlock, { { MidiMessage::controllerEvent (1, 74, 127), 0 } });
    REQUIRE (std::abs (learn.getOverride (cutoff) - 1.0f) < 1.0e-6f);
    f.processor.serviceMessageThread();
    REQUIRE (getParam (f.processor, "flt_cutoff") == Approx (20000.0f));

    process (f.processor, kBlock);
    REQUIRE (std::isnan (learn.getOverride (cutoff))); // cleared once the parameter caught up
}

TEST_CASE ("Processor: learned CCs apply with pickup by default", "[plugin][processor][learn]")
{
    Prepared f;
    const int cutoff = f.processor.getParameterIndex ("flt_cutoff");
    f.processor.getMidiLearn().setMapping (74, cutoff);
    auto& param = *f.processor.getParameterByIndex (cutoff);
    param.setValueNotifyingHost (0.5f);

    for (int value : { 10, 20, 30 })
    {
        process (f.processor, kBlock, { { MidiMessage::controllerEvent (1, 74, value), 0 } });
        f.processor.serviceMessageThread();
        REQUIRE (param.getValue() == Approx (0.5f));
    }

    process (f.processor, kBlock, { { MidiMessage::controllerEvent (1, 74, 64), 0 } });
    f.processor.serviceMessageThread();
    REQUIRE (param.getValue() == Approx (64.0f / 127.0f));
}

TEST_CASE ("Processor: program change only when enabled", "[plugin][processor]")
{
    Prepared f;
    logLines (f.processor, f.logPosition);

    process (f.processor, kBlock, { { MidiMessage::programChange (1, 3), 0 } });
    REQUIRE_FALSE (containsLine (logLines (f.processor, f.logPosition), "Program change"));

    f.processor.setProgramChangeEnabled (true);
    process (f.processor, kBlock, { { MidiMessage::programChange (1, 3), 0 } });
    REQUIRE (containsLine (logLines (f.processor, f.logPosition), "INFO Program change 3 received"));
}

TEST_CASE ("Processor: test tone is a 440 Hz sine at -12 dBFS", "[plugin][processor]")
{
    Prepared f;
    f.processor.startTestTone();

    juce::AudioBuffer<float> out (2, 48000);
    juce::MidiBuffer none;
    f.processor.processBlock (out, none);

    REQUIRE (peak (out) == Approx (0.2512f).margin (0.002));
    REQUIRE (frequency (out, kRate) == Approx (440.0).epsilon (0.002));

    f.processor.processBlock (out, none);
    REQUIRE (peak (f.run (1.1)) == 0.0f); // 2 s long
}

TEST_CASE ("Processor: on-screen/computer keyboard notes play on the active channel", "[plugin][processor]")
{
    Prepared f;
    f.processor.setMidiChannel (3);
    f.processor.injectNoteOn (60, 100);
    process (f.processor, kBlock);

    REQUIRE (f.processor.isNoteHeld (60));
    REQUIRE (peak (f.run (0.02)) > 0.0f);

    f.processor.injectNoteOff (60);
    process (f.processor, kBlock);
    REQUIRE_FALSE (f.processor.isNoteHeld (60));
    REQUIRE (peak (f.run (0.2)) == 0.0f);
}

TEST_CASE ("Processor: MIDI monitor records messages before the channel filter", "[plugin][processor]")
{
    Prepared f;
    f.processor.setMidiChannel (2);
    const auto before = f.processor.getMidiActivityCount();

    process (f.processor, kBlock, { { MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0 },
                                     { MidiMessage::noteOn (2, 62, (juce::uint8) 100), 1 },
                                     { MidiMessage::noteOn (2, 20, (juce::uint8) 100), 2 } });
    f.processor.serviceMessageThread();

    const auto& history = f.processor.getMonitorHistory();
    REQUIRE (history.size() == 3);
    REQUIRE (history[0].dimmed);       // wrong channel
    REQUIRE_FALSE (history[1].dimmed);
    REQUIRE (history[2].dimmed);       // out of range
    REQUIRE (f.processor.getMidiActivityCount() - before == 2); // LED: after the channel filter
}

TEST_CASE ("Processor: odd block sizes, including 0 and larger than prepared", "[plugin][processor]")
{
    Prepared f;
    process (f.processor, kBlock, { { MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0 } });

    for (int size : { 0, 1, 3, 511, 4096, 8192 })
    {
        const auto out = process (f.processor, size, { { MidiMessage::noteOn (1, 64, (juce::uint8) 90), size } });
        REQUIRE (out.getNumSamples() == size);

        for (int i = 0; i < size; ++i)
            REQUIRE (std::isfinite (out.getSample (0, i)));
    }
}

TEST_CASE ("Processor: chords never reach full scale; the safety clipper catches 10 voices", "[plugin][processor]")
{
    waitForFreshRateWindow();

    auto chordPeak = [] (P5XAudioProcessor& p, std::initializer_list<int> notes)
    {
        juce::MidiBuffer midi;

        for (int n : notes)
            midi.addEvent (MidiMessage::noteOn (1, n, (juce::uint8) 100), 0);

        juce::AudioBuffer<float> buffer (2, kBlock);
        p.processBlock (buffer, midi);
        float result = peak (buffer);

        for (int block = 0; block < 500; ++block) // 5 s
        {
            juce::MidiBuffer none;
            p.processBlock (buffer, none);
            result = std::max (result, peak (buffer));
        }

        return result;
    };

    for (const auto& chord : { std::initializer_list<int> { 60, 64, 67, 72, 76 }, std::initializer_list<int> { 60, 61, 62, 63, 64 } })
    {
        // Default 5 voices can never reach the clipper knee (5 × 0.16 = 0.8).
        Prepared f;
        logLines (f.processor, f.logPosition);
        REQUIRE (chordPeak (f.processor, chord) <= 0.8f);
        REQUIRE_FALSE (containsLine (logLines (f.processor, f.logPosition), "Safety clipper engaged"));
    }

    {
        Prepared f;
        setParam (f.processor, "perf_voices", 2.0f); // "10"
        process (f.processor, kBlock);
        waitForFreshRateWindow(); // the 5-voice case above may have used this second's WARN
        logLines (f.processor, f.logPosition);

        REQUIRE (chordPeak (f.processor, { 48, 52, 55, 60, 64, 67, 72, 76, 79, 84 }) < 1.0f);
        REQUIRE (containsLine (logLines (f.processor, f.logPosition), "WARN Safety clipper engaged"));
    }
}

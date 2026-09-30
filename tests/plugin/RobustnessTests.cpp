// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
//
// 11-testing.md § Test types 3 and 4: random parameters + random MIDI at 44.1/48/96/192 kHz and
// block sizes 1, 17, 64, 512, 4096: no NaN/Inf, peak < 4.0, and zero allocations inside
// processBlock after the first block. 5 s of audio per case (DECISIONS.md).

#include "dsp/Random.h"
#include "plugin/TestProcessor.h"
#include "support/AllocationCounter.h"

#include <catch2/catch_test_macros.hpp>

namespace
{
constexpr double kSecondsPerCase = 5.0;

struct Stats
{
    bool finite = true;
    float peak = 0.0f;
    int64_t allocations = 0;
};

// Adds up to a few random events in this block, about 40 per second overall.
void addRandomMidi (juce::MidiBuffer& midi, p5x::Random& random, int blockSize, double sampleRate)
{
    const double expected = 40.0 * blockSize / sampleRate;
    int events = expected >= 1.0 ? (int) (random.nextDouble() * 2.0 * expected) : (random.nextDouble() < expected ? 1 : 0);
    events = juce::jmin (events, 8);

    for (int e = 0; e < events; ++e)
    {
        const int position = (int) (random.nextDouble() * blockSize);
        const int channel = 1 + (int) (random.nextDouble() * 2.0);
        const int note = 20 + (int) (random.nextDouble() * 90.0);
        const int value = (int) (random.nextDouble() * 128.0) & 0x7F;
        const double kind = random.nextDouble();

        juce::MidiMessage m;

        if (kind < 0.35)
            m = juce::MidiMessage::noteOn (channel, note, (juce::uint8) juce::jmax (1, value));
        else if (kind < 0.65)
            m = juce::MidiMessage::noteOff (channel, note);
        else if (kind < 0.75)
            m = juce::MidiMessage::controllerEvent (channel, 64, value);
        else if (kind < 0.85)
            m = juce::MidiMessage::pitchWheel (channel, (int) (random.nextDouble() * 16384.0) & 0x3FFF);
        else if (kind < 0.9)
            m = juce::MidiMessage::controllerEvent (channel, (int) (random.nextDouble() * 128.0) & 0x7F, value);
        else if (kind < 0.95)
            m = juce::MidiMessage::channelPressureChange (channel, value);
        else
            m = juce::MidiMessage::aftertouchChange (channel, note, value);

        midi.addEvent (m, position);
    }
}

Stats runCase (double sampleRate, int blockSize, bool variableBlocks, uint64_t seed)
{
    P5XAudioProcessor processor;
    processor.prepareToPlay (sampleRate, blockSize);

    // Map a few CCs so random CCs exercise MIDI Learn on the audio thread too.
    processor.getMidiLearn().setMapping (74, processor.getParameterIndex ("master_tune"));
    processor.getMidiLearn().setMapping (20, processor.getParameterIndex ("perf_voices"));

    p5x::Random random (seed);
    juce::AudioBuffer<float> buffer (2, blockSize);
    juce::MidiBuffer midi;
    midi.ensureSize (4096);

    Stats stats;
    const int64_t totalSamples = (int64_t) (kSecondsPerCase * sampleRate);
    const int paramEvery = juce::jmax (1, (int) (0.05 * sampleRate / blockSize)); // ~20 changes per second
    int64_t rendered = 0;
    int block = 0;

    while (rendered < totalSamples)
    {
        const int n = variableBlocks ? 1 + (int) (random.nextDouble() * blockSize) : blockSize;
        buffer.setSize (2, n, false, false, true);
        midi.clear();
        addRandomMidi (midi, random, n, sampleRate);

        // Host-side parameter changes happen outside processBlock (they may allocate in JUCE).
        if (block % paramEvery == 0)
        {
            const int index = (int) (random.nextDouble() * p5x::params::kNumParameters);
            processor.getParameterByIndex (index)->setValueNotifyingHost ((float) random.nextDouble());
        }

        {
            p5x::test::ScopedAllocationCounter counter;
            processor.processBlock (buffer, midi);

            if (block > 0)
                stats.allocations += counter.count();
        }

        processor.serviceMessageThread(); // drains the MIDI Learn host FIFO, as the 60 Hz timer would

        for (int ch = 0; ch < 2; ++ch)
        {
            const float* data = buffer.getReadPointer (ch);

            for (int i = 0; i < n; ++i)
            {
                stats.finite = stats.finite && std::isfinite (data[i]);
                stats.peak = std::max (stats.peak, std::abs (data[i]));
            }
        }

        rendered += n;
        ++block;
    }

    return stats;
}
} // namespace

TEST_CASE ("Robustness: random params + MIDI at every rate and block size", "[plugin][robustness][rt][slow]")
{
    uint64_t seed = 1;

    for (double rate : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        for (int blockSize : { 1, 17, 64, 512, 4096 })
        {
            INFO ("rate " << rate << ", block " << blockSize);
            const auto stats = runCase (rate, blockSize, false, seed++);

            REQUIRE (stats.finite);
            REQUIRE (stats.peak < 4.0f);
            REQUIRE (stats.allocations == 0);
        }
    }
}

TEST_CASE ("Robustness: variable block sizes between calls", "[plugin][robustness][rt]")
{
    for (double rate : { 44100.0, 96000.0 })
    {
        INFO ("rate " << rate);
        const auto stats = runCase (rate, 1024, true, 77);

        REQUIRE (stats.finite);
        REQUIRE (stats.peak < 4.0f);
        REQUIRE (stats.allocations == 0);
    }
}

TEST_CASE ("Robustness: repeated prepare, release and re-prepare", "[plugin][robustness]")
{
    P5XAudioProcessor processor;

    for (int round = 0; round < 5; ++round)
    {
        const double rate = round % 2 == 0 ? 48000.0 : 96000.0;
        processor.prepareToPlay (rate, 64 << round);
        processor.prepareToPlay (rate, 256);

        const auto out = p5x::test::process (processor, 256, { { juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0 } });
        REQUIRE (p5x::test::peak (out) > 0.0f);

        processor.releaseResources();
    }
}

// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
//
// Golden renders (11-testing.md § Test types 2): tests/data/golden.mid rendered offline with seed 1 at
// 48 kHz for a set of test patches, compared with the stored reference WAVs by RMS per 50 ms window
// (±0.5 dB) and spectral centroid (±3 %). Not bit-exact.
//
// Writing new references needs the user's approval and a DECISIONS.md line; it happens only with
// P5X_UPDATE_GOLDEN set (see below). Regenerate golden.mid with the hidden test "[.generate]".

#include "plugin/TestProcessor.h"
#include "support/Spectrum.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <catch2/catch_test_macros.hpp>

#include <cstdio>

namespace
{
constexpr double kRate = 48000.0;
constexpr int kBlock = 480;
constexpr uint64_t kSeed = 1;

const juce::File dataDir = juce::File (P5X_TEST_SOURCE_DIR).getChildFile ("data");

struct Patch
{
    const char* name;
    std::vector<std::pair<const char*, float>> values; // real units; everything else at its default
};

// Test patches (presets arrive in milestone 6). Each exercises a different part of the voice.
const std::vector<Patch>& patches()
{
    static const std::vector<Patch> list {
        { "init", {} },
        { "bright_resonant", { { "flt_cutoff", 8000.0f }, { "flt_res", 0.6f }, { "flt_env_amt", 0.7f },
                               { "fenv_decay", 0.3f }, { "fenv_sustain", 0.2f } } },
        { "pulse_triangle", { { "osc_a_saw", 0.0f }, { "osc_a_pulse", 1.0f }, { "osc_a_pw", 0.3f }, { "osc_b_saw", 0.0f },
                              { "osc_b_tri", 1.0f }, { "osc_b_freq", 12.0f }, { "flt_cutoff", 2000.0f },
                              { "flt_res", 0.3f } } },
        { "noise_percussive", { { "mix_osc_a", 0.4f }, { "mix_osc_b", 0.0f }, { "mix_noise", 0.8f },
                                { "aenv_attack", 0.001f }, { "aenv_decay", 0.25f }, { "aenv_sustain", 0.0f },
                                { "aenv_release", 0.1f }, { "flt_cutoff", 3000.0f }, { "flt_res", 0.5f } } },
        { "kbd_off_tracking", { { "osc_b_kbd", 0.0f }, { "osc_b_fine", 7.0f }, { "flt_kbd", 2.0f },
                                { "flt_cutoff", 1500.0f }, { "flt_env_amt", 0.2f } } },
        // Milestone 3: classic sync sweep (filter envelope → Freq A) with a touch of Osc B FM.
        { "sync_polymod_fm", { { "osc_a_sync", 1.0f }, { "osc_a_freq", 7.0f }, { "mix_osc_b", 0.0f },
                               { "pm_filt_env", 0.4f }, { "pm_osc_b", 0.1f }, { "pm_dest_freq_a", 1.0f },
                               { "flt_cutoff", 6000.0f }, { "flt_res", 0.2f }, { "fenv_decay", 0.8f },
                               { "fenv_sustain", 0.1f } } },
        // Milestone 3: Osc B as a slow Lo Freq, Kbd off triangle, sweeping the filter and A's PW.
        { "polymod_filter_lofreq", { { "osc_b_lofreq", 1.0f }, { "osc_b_kbd", 0.0f }, { "osc_b_saw", 0.0f },
                                     { "osc_b_tri", 1.0f }, { "mix_osc_b", 0.0f }, { "osc_a_saw", 0.0f },
                                     { "osc_a_pulse", 1.0f }, { "pm_osc_b", 0.5f }, { "pm_dest_filter", 1.0f },
                                     { "pm_dest_pw_a", 1.0f }, { "flt_cutoff", 800.0f }, { "flt_res", 0.5f },
                                     { "flt_env_amt", 0.1f } } },
    };

    return list;
}

// The fixed sequence: chords, a legato line, sustain use and bends (times in seconds).
juce::MidiMessageSequence goldenSequence()
{
    juce::MidiMessageSequence seq;
    auto note = [&seq] (double on, double off, int n, int velocity = 100)
    {
        seq.addEvent (juce::MidiMessage::noteOn (1, n, (juce::uint8) velocity), on);
        seq.addEvent (juce::MidiMessage::noteOff (1, n), off);
    };

    for (int n : { 48, 52, 55, 60 })
        note (0.0, 1.0, n);

    for (int i = 0; i < 5; ++i) // overlapping legato line
        note (1.2 + i * 0.25, 1.2 + (i + 1) * 0.25 + 0.05, std::array<int, 5> { 60, 62, 64, 65, 67 }[(size_t) i]);

    seq.addEvent (juce::MidiMessage::controllerEvent (1, 64, 127), 2.8);
    note (2.8, 3.1, 57);
    note (2.9, 3.1, 60);
    note (3.0, 3.1, 64);
    seq.addEvent (juce::MidiMessage::controllerEvent (1, 64, 0), 4.0);

    note (4.3, 5.6, 67);

    for (int step = 0; step <= 50; ++step) // bend up and back over 1 s
    {
        const double t = step / 50.0;
        const int value = 8192 + (int) (8191.0 * (t < 0.5 ? 2.0 * t : 2.0 - 2.0 * t));
        seq.addEvent (juce::MidiMessage::pitchWheel (1, value), 4.4 + t);
    }

    for (int n : { 48, 55, 60, 64, 67 })
        note (5.8, 6.8, n);

    seq.updateMatchedPairs();
    seq.sort();
    return seq;
}

juce::MidiMessageSequence loadGoldenMidi()
{
    juce::FileInputStream in (dataDir.getChildFile ("golden.mid"));
    juce::MidiFile file;
    REQUIRE (in.openedOk());
    REQUIRE (file.readFrom (in));
    file.convertTimestampTicksToSeconds();
    REQUIRE (file.getNumTracks() >= 1);
    return *file.getTrack (0);
}

std::vector<float> renderPatch (const Patch& patch, const juce::MidiMessageSequence& seq)
{
    P5XAudioProcessor p (false);

    // Seed 1, via the state (as a host would restore it).
    juce::MemoryBlock state;
    p.getStateInformation (state);
    auto xml = P5XAudioProcessor::getXmlFromBinary (state.getData(), (int) state.getSize());
    xml->getChildByName ("Seed")->setAttribute ("value", juce::String (kSeed));
    juce::AudioProcessor::copyXmlToBinary (*xml, state);
    p.setStateInformation (state.getData(), (int) state.getSize());

    for (const auto& [id, value] : patch.values)
        p5x::test::setParam (p, id, value);

    p.prepareToPlay (kRate, kBlock);

    const auto totalSamples = (int) (7.6 * kRate);
    std::vector<float> out;
    out.reserve ((size_t) totalSamples);
    int eventIndex = 0;

    for (int start = 0; start < totalSamples; start += kBlock)
    {
        juce::MidiBuffer midi;

        while (eventIndex < seq.getNumEvents())
        {
            const auto& m = seq.getEventPointer (eventIndex)->message;
            const auto position = (int) std::lround (m.getTimeStamp() * kRate);

            if (position >= start + kBlock)
                break;

            midi.addEvent (m, juce::jmax (0, position - start));
            ++eventIndex;
        }

        juce::AudioBuffer<float> buffer (2, kBlock);
        p.processBlock (buffer, midi);
        out.insert (out.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + kBlock);
    }

    return out;
}

double spectralCentroid (const std::vector<float>& x)
{
    constexpr size_t n = 4096;
    std::vector<double> power (n / 2 + 1, 0.0);

    for (size_t start = 0; start + n <= x.size(); start += n)
    {
        const auto mag = p5x::test::magnitudeSpectrum (std::vector<float> (x.begin() + (long) start, x.begin() + (long) (start + n)));

        for (size_t k = 0; k < mag.size(); ++k)
            power[k] += mag[k] * mag[k];
    }

    double weighted = 0.0, total = 0.0;

    for (size_t k = 1; k < power.size(); ++k)
    {
        weighted += power[k] * (double) k * kRate / (double) n;
        total += power[k];
    }

    return total > 0.0 ? weighted / total : 0.0;
}

bool writeWav (const juce::File& file, const std::vector<float>& x)
{
    file.getParentDirectory().createDirectory();
    file.deleteFile();
    std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();

    if (stream == nullptr)
        return false;

    juce::WavAudioFormat wav;
    const auto options = juce::AudioFormatWriterOptions()
                             .withSampleRate (kRate)
                             .withNumChannels (1)
                             .withBitsPerSample (32)
                             .withSampleFormat (juce::AudioFormatWriterOptions::SampleFormat::floatingPoint);
    auto writer = wav.createWriterFor (stream, options);

    if (writer == nullptr)
        return false;

    const float* channels[] = { x.data() };
    return writer->writeFromFloatArrays (channels, 1, (int) x.size());
}

std::vector<float> readWav (const juce::File& file)
{
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatReader> reader (wav.createReaderFor (file.createInputStream().release(), true));

    if (reader == nullptr)
        return {};

    juce::AudioBuffer<float> buffer (1, (int) reader->lengthInSamples);
    reader->read (&buffer, 0, (int) reader->lengthInSamples, 0, true, false);
    return { buffer.getReadPointer (0), buffer.getReadPointer (0) + buffer.getNumSamples() };
}
} // namespace

TEST_CASE ("Golden: write tests/data/golden.mid", "[.generate]")
{
    juce::MidiFile file;
    file.setSmpteTimeFormat (25, 40); // 1 000 ticks per second
    auto seq = goldenSequence();

    for (int i = 0; i < seq.getNumEvents(); ++i)
    {
        auto& m = seq.getEventPointer (i)->message;
        m.setTimeStamp (std::round (m.getTimeStamp() * 1000.0));
    }

    file.addTrack (seq);
    dataDir.createDirectory();
    const auto target = dataDir.getChildFile ("golden.mid");
    target.deleteFile();
    juce::FileOutputStream out (target);
    REQUIRE (out.openedOk());
    REQUIRE (file.writeTo (out));
}

TEST_CASE ("Golden renders match their references", "[plugin][golden]")
{
    const auto seq = loadGoldenMidi();

    // "1" rewrites every reference; a comma-separated list of patch names writes only those (for
    // adding a new patch without touching the approved ones).
    const auto updateSetting = juce::SystemStats::getEnvironmentVariable ("P5X_UPDATE_GOLDEN", {});
    const auto updateNames = juce::StringArray::fromTokens (updateSetting, ",", {});

    for (const auto& patch : patches())
    {
        INFO ("patch " << patch.name);
        const auto rendered = renderPatch (patch, seq);
        const auto reference = dataDir.getChildFile ("golden").getChildFile (juce::String (patch.name) + ".wav");

        for (float s : rendered)
            REQUIRE (std::isfinite (s));

        REQUIRE (p5x::test::rms (rendered) > 1.0e-3); // the patch actually sounds

        if (updateSetting == "1" || updateNames.contains (patch.name))
        {
            REQUIRE (writeWav (reference, rendered));
            std::printf ("golden: wrote %s\n", reference.getFullPathName().toRawUTF8());
            continue;
        }

        const auto expected = readWav (reference);
        REQUIRE (expected.size() == rendered.size());

        // RMS per 50 ms window within ±0.5 dB (windows quieter than -70 dBFS in both are skipped).
        const size_t window = (size_t) (0.05 * kRate);

        for (size_t start = 0; start + window <= rendered.size(); start += window)
        {
            const double a = p5x::test::rms (rendered, start, start + window);
            const double b = p5x::test::rms (expected, start, start + window);

            if (p5x::test::toDb (a) < -70.0 && p5x::test::toDb (b) < -70.0)
                continue;

            INFO ("window at " << (double) start / kRate << " s");
            REQUIRE (std::abs (p5x::test::toDb (a / b)) <= 0.5);
        }

        const double centroid = spectralCentroid (rendered), expectedCentroid = spectralCentroid (expected);
        REQUIRE (std::abs (centroid / expectedCentroid - 1.0) <= 0.03);
    }
}

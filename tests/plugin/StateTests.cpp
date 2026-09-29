// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#include "AppPaths.h"
#include "dsp/Random.h"
#include "plugin/TestProcessor.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace p5x::test;
using Catch::Approx;

namespace
{
// No saved defaults from other tests leak into these.
void clearGlobalFiles()
{
    p5x::paths::settingsFile().deleteFile();
    p5x::paths::midiMapFile().deleteFile();
}

juce::MemoryBlock stateOf (P5XAudioProcessor& p)
{
    juce::MemoryBlock block;
    p.getStateInformation (block);
    return block;
}

void loadXmlString (P5XAudioProcessor& p, const juce::String& xmlText)
{
    juce::MemoryBlock block;
    juce::AudioProcessor::copyXmlToBinary (*juce::parseXML (xmlText), block);
    p.setStateInformation (block.getData(), (int) block.getSize());
}

bool allDefaults (P5XAudioProcessor& p)
{
    for (int i = 0; i < p5x::params::kNumParameters; ++i)
    {
        auto* param = p.getParameterByIndex (i);

        if (std::abs (param->getValue() - param->getDefaultValue()) > 1.0e-6f)
            return false;
    }

    return true;
}

void scramble (P5XAudioProcessor& p, uint64_t seed)
{
    p5x::Random random (seed);

    for (int i = 0; i < p5x::params::kNumParameters; ++i)
        p.getParameterByIndex (i)->setValueNotifyingHost ((float) random.nextDouble());
}
} // namespace

TEST_CASE ("State: full round trip (params, map, settings, seed)", "[plugin][state]")
{
    clearGlobalFiles();
    P5XAudioProcessor a;
    scramble (a, 7);
    a.getMidiLearn().setMapping (74, a.getParameterIndex ("flt_cutoff"));
    a.getMidiLearn().setMapping (71, a.getParameterIndex ("flt_res"));
    a.getMidiLearn().setMapping (20, a.getParameterIndex ("perf_unison"));
    a.setMidiChannel (5);
    a.setTakeover (p5x::midi::Takeover::Jump);
    a.setHqMode (true);
    a.setProgramChangeEnabled (true);

    const auto state = stateOf (a);

    P5XAudioProcessor b;
    REQUIRE (b.getSeed() != a.getSeed());
    b.setStateInformation (state.getData(), (int) state.getSize());

    for (int i = 0; i < p5x::params::kNumParameters; ++i)
    {
        INFO (p5x::params::kAllIds[(size_t) i]);
        REQUIRE (b.getParameterByIndex (i)->getValue() == Approx (a.getParameterByIndex (i)->getValue()).margin (1.0e-5));
    }

    REQUIRE (b.getMidiLearn().getMap() == a.getMidiLearn().getMap());
    REQUIRE (b.getMidiChannel() == 5);
    REQUIRE (b.getTakeover() == p5x::midi::Takeover::Jump);
    REQUIRE (b.isHqMode());
    REQUIRE (b.isProgramChangeEnabled());
    REQUIRE (b.getSeed() == a.getSeed());
}

TEST_CASE ("State: garbage loads defaults and never crashes", "[plugin][state]")
{
    clearGlobalFiles();
    P5XAudioProcessor p;
    p5x::Random random (99);

    SECTION ("random bytes")
    {
        for (int round = 0; round < 50; ++round)
        {
            scramble (p, (uint64_t) round);
            std::vector<uint8_t> bytes ((size_t) (1 + random.nextDouble() * 2000));

            for (auto& b : bytes)
                b = (uint8_t) (random.nextUInt64() & 0xFF);

            p.setStateInformation (bytes.data(), (int) bytes.size());
            REQUIRE (allDefaults (p));
        }
    }

    SECTION ("empty, null and truncated")
    {
        scramble (p, 1);
        p.setStateInformation (nullptr, 0);
        REQUIRE (allDefaults (p));

        scramble (p, 2);
        const auto good = stateOf (p);
        p.setStateInformation (good.getData(), (int) good.getSize() / 2);
        REQUIRE (allDefaults (p));
    }

    SECTION ("missing, invalid or zero version, or the wrong root element")
    {
        for (const char* xml : { "<P5XState><PARAMETERS/></P5XState>",
                                 "<P5XState version=\"abc\"/>",
                                 "<P5XState version=\"0\"/>",
                                 "<P5XState version=\"-3\"/>",
                                 "<SomethingElse version=\"1\"/>" })
        {
            scramble (p, 3);
            loadXmlString (p, xml);
            INFO (xml);
            REQUIRE (allDefaults (p));
        }
    }
}

TEST_CASE ("State: fuzzed state XML never crashes", "[plugin][state]")
{
    clearGlobalFiles();
    P5XAudioProcessor p;
    scramble (p, 5);
    p.getMidiLearn().setMapping (74, 16);
    const auto good = stateOf (p);
    const auto xmlText = P5XAudioProcessor::getXmlFromBinary (good.getData(), (int) good.getSize())->toString();
    p5x::Random random (1234);

    for (int round = 0; round < 300; ++round)
    {
        auto mutated = xmlText;

        for (int edits = 0; edits < 5; ++edits)
        {
            const int pos = (int) (random.nextDouble() * mutated.length());
            const auto ch = (juce::juce_wchar) (32 + (int) (random.nextDouble() * 90));
            mutated = mutated.substring (0, pos) + juce::String::charToString (ch) + mutated.substring (pos + 1);
        }

        if (auto xml = juce::parseXML (mutated))
        {
            juce::MemoryBlock block;
            juce::AudioProcessor::copyXmlToBinary (*xml, block);
            p.setStateInformation (block.getData(), (int) block.getSize());
        }
    }

    // Still a working instance.
    const auto out = process (p, 64);
    REQUIRE (out.getNumSamples() == 64);
}

TEST_CASE ("State: tolerant parsing (missing, invalid, out of range, unknown, newer version)", "[plugin][state]")
{
    clearGlobalFiles();
    P5XAudioProcessor p;
    scramble (p, 11);
    uint64_t logPosition = 0;
    logLines (p, logPosition);

    loadXmlString (p, R"(<P5XState version="2">
        <PARAMETERS>
            <PARAM id="flt_cutoff" value="99999"/>
            <PARAM id="flt_res" value="banana"/>
            <PARAM id="osc_a_freq" value="7"/>
            <PARAM id="future_param" value="1"/>
        </PARAMETERS>
        <FutureElement something="1"/>
        <Settings midiChannel="40" takeover="jump" futureSetting="x"/>
        <Seed value="not a number"/>
    </P5XState>)");

    REQUIRE (getParam (p, "flt_cutoff") == Approx (20000.0f)); // clamped
    REQUIRE (getParam (p, "flt_res") == Approx (0.0f));        // invalid → default
    REQUIRE (getParam (p, "osc_a_freq") == Approx (7.0f));
    REQUIRE (getParam (p, "mix_osc_b") == Approx (0.8f));      // missing → default
    REQUIRE (p.getMidiChannel() == 16);                        // clamped
    REQUIRE (p.getTakeover() == p5x::midi::Takeover::Jump);
    REQUIRE (containsLine (logLines (p, logPosition), "WARN State version 2 is newer"));
}

TEST_CASE ("State: <MidiMap> presence decides the map", "[plugin][state]")
{
    clearGlobalFiles();
    P5XAudioProcessor p;
    p.getMidiLearn().setMapping (74, p.getParameterIndex ("flt_cutoff"));

    // No <MidiMap>: the current (instance/default) map stays, as with a preset load.
    loadXmlString (p, R"(<P5XState version="1"><PARAMETERS/></P5XState>)");
    REQUIRE (p.getMidiLearn().getNumMappings() == 1);

    // An empty <MidiMap/> is a map: cleared stays cleared.
    loadXmlString (p, R"(<P5XState version="1"><MidiMap/></P5XState>)");
    REQUIRE (p.getMidiLearn().getNumMappings() == 0);
}

TEST_CASE ("State: the saved default map and settings apply to new instances", "[plugin][state]")
{
    clearGlobalFiles();

    {
        P5XAudioProcessor p;
        p.getMidiLearn().setMapping (74, p.getParameterIndex ("flt_cutoff"));
        REQUIRE (p.saveMidiMapAsDefault());
        p.setMidiChannel (9);
        p.setProgramChangeEnabled (true);
        p.saveSettingsAsDefault();
    }

    P5XAudioProcessor fresh;
    REQUIRE (fresh.getMidiLearn().getCcForParam (fresh.getParameterIndex ("flt_cutoff")) == 74);
    REQUIRE (fresh.getMidiChannel() == 9);
    REQUIRE (fresh.isProgramChangeEnabled());

    clearGlobalFiles();
}

TEST_CASE ("State: set before and after prepareToPlay", "[plugin][state]")
{
    clearGlobalFiles();
    P5XAudioProcessor a;
    setParam (a, "master_volume", -6.0f);
    const auto state = stateOf (a);

    P5XAudioProcessor b;
    b.setStateInformation (state.getData(), (int) state.getSize());
    b.prepareToPlay (48000.0, 256);
    REQUIRE (getParam (b, "master_volume") == Approx (-6.0f));

    b.setStateInformation (state.getData(), (int) state.getSize());
    b.releaseResources();
    b.prepareToPlay (44100.0, 128);
    REQUIRE (getParam (b, "master_volume") == Approx (-6.0f));
}

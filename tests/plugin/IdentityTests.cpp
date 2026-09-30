// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#include "AppPaths.h"
#include "PluginProcessor.h"

#include <catch2/catch_test_macros.hpp>

// JUCE defines the bundle ID as bare tokens, not a string literal.
#define P5X_STRINGIFY_IMPL(x) #x
#define P5X_STRINGIFY(x) P5X_STRINGIFY_IMPL (x)

// CLAUDE.md § Product identity: these must never change after milestone 1.
TEST_CASE ("Identity: vendor, name, version and codes", "[plugin][identity]")
{
    REQUIRE (juce::String (JucePlugin_Manufacturer) == "Jeet Patel");
    REQUIRE (juce::String (JucePlugin_Name) == "P5X");
    REQUIRE (juce::String (JucePlugin_VersionString) == "0.3.0");
    REQUIRE (JucePlugin_ManufacturerCode == 0x4a74506c); // 'JtPl'
    REQUIRE (JucePlugin_PluginCode == 0x50357853);       // 'P5xS'
    REQUIRE (juce::String (P5X_STRINGIFY (JucePlugin_CFBundleIdentifier)) == "com.jeetpatel.p5x");
    REQUIRE (JucePlugin_IsSynth == 1);
    REQUIRE (JucePlugin_WantsMidiInput == 1);
    REQUIRE (JucePlugin_ProducesMidiOutput == 0);
    REQUIRE (juce::String (JucePlugin_Vst3Category) == "Instrument|Synth");
}

TEST_CASE ("Identity: the processor reports the same", "[plugin][identity]")
{
    P5XAudioProcessor processor;

    REQUIRE (processor.getName() == "P5X");
    REQUIRE (processor.acceptsMidi());
    REQUIRE_FALSE (processor.producesMidi());
    REQUIRE (processor.getTotalNumInputChannels() == 0);
    REQUIRE (processor.getTotalNumOutputChannels() == 2);
    REQUIRE (processor.hasEditor());
}

TEST_CASE ("Tests keep global files out of the user's AppData", "[plugin][identity]")
{
    const juce::File expected (P5X_TEST_DATA_DIR);
    REQUIRE (p5x::paths::appDataDir() == expected);
}

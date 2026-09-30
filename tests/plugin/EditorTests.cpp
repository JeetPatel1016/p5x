// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#include "PluginProcessor.h"
#include "debug/DebugConsole.h"
#include "standalone/SettingsDialog.h"

#include <catch2/catch_test_macros.hpp>

#include <set>

namespace
{
void collectTitles (juce::Component& component, std::multiset<juce::String>& titles)
{
    if (component.getTitle().isNotEmpty())
        titles.insert (component.getTitle());

    for (auto* child : component.getChildren())
        collectTitles (*child, titles);
}
} // namespace

TEST_CASE ("Placeholder editor: exactly one control per parameter", "[plugin][ui]")
{
    P5XAudioProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditorAndMakeActive());
    REQUIRE (editor != nullptr);
    REQUIRE (editor->getWidth() > 0);
    REQUIRE (editor->getHeight() > 0);

    std::multiset<juce::String> titles;
    collectTitles (*editor, titles);

    for (int i = 0; i < p5x::params::kNumParameters; ++i)
    {
        const auto name = processor.getParameterByIndex (i)->getName (100);
        INFO (name);
        REQUIRE (titles.count (name) == 1);
    }

    editor.reset();
}

TEST_CASE ("Debug console and settings dialog build without a window", "[plugin][ui]")
{
    P5XAudioProcessor processor;

    {
        p5x::debug::DebugConsole console (processor);
        REQUIRE (console.getWidth() > 0);
    }

    {
        p5x::standalone::SettingsDialog settings (processor);
        REQUIRE (settings.getHeight() > 0); // plugin build: MIDI and HQ items only
    }
}

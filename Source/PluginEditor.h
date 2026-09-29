// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#pragma once

#include "PluginProcessor.h"
#include "ui/LookAndFeelP5X.h"
#include "ui/OnScreenKeyboard.h"
#include "ui/PlaceholderPanel.h"

#include <juce_audio_processors/juce_audio_processors.h>

// Milestones 1–4: top strip + placeholder grid + on-screen keyboard (08-ui.md § Placeholder editor).
class P5XAudioProcessorEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit P5XAudioProcessorEditor (P5XAudioProcessor&);
    ~P5XAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& key) override;

    void showSettings();
    void setDebugConsoleVisible (bool visible);

private:
    class MidiLed;
    class ToolWindow;

    void timerCallback() override;

    P5XAudioProcessor& processor;
    p5x::ui::LookAndFeelP5X lookAndFeel;

    juce::TextButton debugButton { "Debug" }, settingsButton { "Settings" };
    std::unique_ptr<MidiLed> midiLed;
    juce::Label cpuLabel, learnLabel;

    p5x::ui::PlaceholderPanel panel;
    p5x::ui::OnScreenKeyboard keyboard;
    juce::TooltipWindow tooltips { this, 500 };

    std::unique_ptr<ToolWindow> consoleWindow, settingsWindow;

    uint32_t lastMidiActivity = 0;
    double ledOffAtMs = 0.0;
    juce::String learnedText;
    double learnedUntilMs = 0.0;
};

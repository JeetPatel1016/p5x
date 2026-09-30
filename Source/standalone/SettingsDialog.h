// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include <functional>
#include <memory>

class P5XAudioProcessor;

namespace p5x::standalone
{
// Audio/MIDI settings (10-debug-and-harness.md § Standalone test harness). Plain functional layout
// until the artboard arrives (OPEN_QUESTIONS #15). In the plugin build it shows only the MIDI and
// HQ items. The Standalone input and "Route input into filter" arrive in milestone 2.
class SettingsDialog : public juce::Component, private juce::Timer
{
public:
    explicit SettingsDialog (P5XAudioProcessor& processor);
    ~SettingsDialog() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    std::function<void()> onClose;

private:
    struct Row
    {
        std::unique_ptr<juce::Label> label;
        juce::Component* control = nullptr;
    };

    void timerCallback() override;
    void addRow (const juce::String& text, juce::Component& control);
    void settingsChanged();
    void confirmClearAll();

    P5XAudioProcessor& processor;
    const bool isStandalone;

    std::unique_ptr<juce::AudioDeviceSelectorComponent> deviceSelector;
    juce::Label audioHeading, midiHeading, footer, hqNote, mappingsLabel;

    juce::ComboBox channelBox, takeoverBox;
    juce::ToggleButton programChangeToggle, hqToggle, computerKeyboardToggle;
    juce::Slider bendRangeSlider;
    std::unique_ptr<juce::SliderParameterAttachment> bendRangeAttachment;
    juce::TextButton saveMapButton { "Save as default map" }, clearMapButton { "Clear all" };
    juce::TextButton testToneButton { "Test tone" }, resetAudioButton { "Reset audio" }, closeButton { "Close" };
    juce::Component mappingsRow;

    std::vector<Row> rows;
    int lastMappingCount = -1;
};
} // namespace p5x::standalone

// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#include "standalone/SettingsDialog.h"
#include "PluginProcessor.h"
#include "standalone/StandaloneServices.h"
#include "ui/LookAndFeelP5X.h"

namespace p5x::standalone
{
namespace
{
std::atomic<Services*> currentServices { nullptr };

constexpr int kWidth = 600;
constexpr int kRowHeight = 30;
constexpr int kLabelWidth = 220;
constexpr int kDeviceSelectorHeight = 340;
} // namespace

Services* getServices() noexcept { return currentServices.load(); }
void setServices (Services* services) noexcept { currentServices.store (services); }

//==================================================================================================
SettingsDialog::SettingsDialog (P5XAudioProcessor& p)
    : processor (p),
      isStandalone (p.wrapperType == juce::AudioProcessor::wrapperType_Standalone && getServices() != nullptr)
{
    auto heading = [this] (juce::Label& label, const juce::String& text)
    {
        label.setText (text, juce::dontSendNotification);
        label.setFont (ui::LookAndFeelP5X::font (14.0f, true));
        label.setColour (juce::Label::textColourId, ui::colours::sectionTitle);
        addAndMakeVisible (label);
    };

    if (isStandalone)
    {
        heading (audioHeading, "AUDIO");
        deviceSelector = std::make_unique<juce::AudioDeviceSelectorComponent> (getServices()->getDeviceManager(),
                                                                               0, 2, 2, 2, true, false, true, false);
        addAndMakeVisible (*deviceSelector);

        // The selected input's first channel goes into every voice's mixer at unity. Off at launch
        // and never saved, to avoid feedback surprises (10-debug-and-harness.md).
        routeInputLabel.setText ("Route input into filter", juce::dontSendNotification);
        routeInputLabel.setColour (juce::Label::textColourId, ui::colours::label);
        addAndMakeVisible (routeInputLabel);
        routeInputToggle.setToggleState (processor.isRoutingInput(), juce::dontSendNotification);
        routeInputToggle.onClick = [this]
        {
            const bool on = routeInputToggle.getToggleState();
            processor.setRouteInput (on);
            getServices()->setInputMuted (! on);
        };
        addAndMakeVisible (routeInputToggle);
    }

    heading (midiHeading, "MIDI");

    // MIDI channel: 0 = Omni, 1–16.
    channelBox.addItem ("Omni", 1);

    for (int ch = 1; ch <= 16; ++ch)
        channelBox.addItem (juce::String (ch), ch + 1);

    channelBox.setSelectedId (processor.getMidiChannel() + 1, juce::dontSendNotification);
    channelBox.onChange = [this]
    {
        processor.setMidiChannel (channelBox.getSelectedId() - 1);
        settingsChanged();
    };
    addRow ("MIDI channel", channelBox);

    takeoverBox.addItem ("Pickup", 1);
    takeoverBox.addItem ("Jump", 2);
    takeoverBox.setSelectedId (processor.getTakeover() == midi::Takeover::Jump ? 2 : 1, juce::dontSendNotification);
    takeoverBox.onChange = [this]
    {
        processor.setTakeover (takeoverBox.getSelectedId() == 2 ? midi::Takeover::Jump : midi::Takeover::Pickup);
        settingsChanged();
    };
    addRow ("Knob takeover", takeoverBox);

    // CC mappings count + Save as default map + Clear all.
    mappingsLabel.setColour (juce::Label::textColourId, ui::colours::text);
    mappingsRow.addAndMakeVisible (mappingsLabel);
    mappingsRow.addAndMakeVisible (saveMapButton);
    mappingsRow.addAndMakeVisible (clearMapButton);
    saveMapButton.onClick = [this] { processor.saveMidiMapAsDefault(); };
    clearMapButton.onClick = [this] { confirmClearAll(); };
    addRow ("CC mappings", mappingsRow);

    programChangeToggle.setToggleState (processor.isProgramChangeEnabled(), juce::dontSendNotification);
    programChangeToggle.onClick = [this]
    {
        processor.setProgramChangeEnabled (programChangeToggle.getToggleState());
        settingsChanged();
    };
    addRow ("Program change", programChangeToggle);

    bendRangeSlider.setSliderStyle (juce::Slider::IncDecButtons);
    bendRangeSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 60, 22);

    if (auto* bend = processor.getParameterByIndex (processor.getParameterIndex (params::id::bendRange)))
        bendRangeAttachment = std::make_unique<juce::SliderParameterAttachment> (*bend, bendRangeSlider);

    bendRangeSlider.setTextValueSuffix (" st");
    addRow ("Pitch bend range", bendRangeSlider);

    hqToggle.setToggleState (processor.isHqMode(), juce::dontSendNotification);
    hqToggle.onClick = [this]
    {
        processor.setHqMode (hqToggle.getToggleState());
        settingsChanged();

        // HQ applies at the next prepareToPlay; the Standalone restarts the device (06-voices.md).
        if (isStandalone)
            getServices()->resetAudio();
    };
    hqNote.setText (isStandalone ? "Restarts the audio device" : "Applies on next playback restart",
                    juce::dontSendNotification);
    hqNote.setColour (juce::Label::textColourId, ui::colours::labelDim);
    addAndMakeVisible (hqNote);
    addRow ("HQ mode (4x oversampling)", hqToggle);

    if (isStandalone)
    {
        computerKeyboardToggle.setToggleState (getServices()->isComputerKeyboardEnabled(), juce::dontSendNotification);
        computerKeyboardToggle.onClick = [this]
        { getServices()->setComputerKeyboardEnabled (computerKeyboardToggle.getToggleState()); };
        addRow ("Computer keyboard plays notes", computerKeyboardToggle);

        resetAudioButton.onClick = [] { getServices()->resetAudio(); };
        addAndMakeVisible (resetAudioButton);
    }

    testToneButton.onClick = [this] { processor.startTestTone(); };
    addAndMakeVisible (testToneButton);
    closeButton.onClick = [this]
    {
        if (onClose)
            onClose();
    };
    addAndMakeVisible (closeButton);

    footer.setText ("P5X " JucePlugin_VersionString + juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 \xc2\xa9 2026 Jeet Patel")),
                    juce::dontSendNotification);
    footer.setColour (juce::Label::textColourId, ui::colours::labelDim);
    footer.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (footer);

    timerCallback();
    startTimerHz (4);

    const int height = (isStandalone ? 30 + kDeviceSelectorHeight + kRowHeight : 0) + 30 + (int) rows.size() * kRowHeight + 110;
    setSize (kWidth, height);
}

SettingsDialog::~SettingsDialog()
{
    stopTimer();
}

void SettingsDialog::addRow (const juce::String& text, juce::Component& control)
{
    Row row;
    row.label = std::make_unique<juce::Label> (juce::String(), text);
    row.label->setColour (juce::Label::textColourId, ui::colours::label);
    addAndMakeVisible (*row.label);
    row.control = &control;
    addAndMakeVisible (control);
    rows.push_back (std::move (row));
}

void SettingsDialog::paint (juce::Graphics& g)
{
    g.fillAll (ui::colours::panelBg);
}

void SettingsDialog::resized()
{
    auto area = getLocalBounds().reduced (16, 12);

    if (deviceSelector != nullptr)
    {
        audioHeading.setBounds (area.removeFromTop (26));
        deviceSelector->setBounds (area.removeFromTop (kDeviceSelectorHeight));
        auto routeRow = area.removeFromTop (kRowHeight).reduced (0, 3);
        routeInputLabel.setBounds (routeRow.removeFromLeft (kLabelWidth));
        routeInputToggle.setBounds (routeRow.removeFromLeft (40));
    }

    midiHeading.setBounds (area.removeFromTop (26));

    for (auto& row : rows)
    {
        auto line = area.removeFromTop (kRowHeight).reduced (0, 3);
        row.label->setBounds (line.removeFromLeft (kLabelWidth));

        if (row.control == &mappingsRow)
        {
            row.control->setBounds (line);
            auto inner = mappingsRow.getLocalBounds();
            mappingsLabel.setBounds (inner.removeFromLeft (40));
            clearMapButton.setBounds (inner.removeFromRight (90));
            inner.removeFromRight (6);
            saveMapButton.setBounds (inner.removeFromRight (160));
        }
        else if (row.control == &hqToggle)
        {
            row.control->setBounds (line.removeFromLeft (40));
            hqNote.setBounds (line);
        }
        else if (row.control == &bendRangeSlider || row.control == &channelBox || row.control == &takeoverBox)
        {
            row.control->setBounds (line.removeFromLeft (150));
        }
        else
        {
            row.control->setBounds (line.removeFromLeft (40));
        }
    }

    footer.setBounds (area.removeFromBottom (22));
    area.removeFromBottom (8);
    auto buttons = area.removeFromBottom (30);
    closeButton.setBounds (buttons.removeFromRight (90));
    testToneButton.setBounds (buttons.removeFromLeft (100));

    if (isStandalone)
    {
        buttons.removeFromLeft (8);
        resetAudioButton.setBounds (buttons.removeFromLeft (110));
    }
}

void SettingsDialog::timerCallback()
{
    const int count = processor.getMidiLearn().getNumMappings();

    if (count != lastMappingCount)
    {
        lastMappingCount = count;
        mappingsLabel.setText (juce::String (count), juce::dontSendNotification);
    }
}

void SettingsDialog::settingsChanged()
{
    processor.saveSettingsAsDefault(); // DECISIONS.md: settings.xml is rewritten on every change
}

void SettingsDialog::confirmClearAll()
{
    auto options = juce::MessageBoxOptions()
                       .withIconType (juce::MessageBoxIconType::QuestionIcon)
                       .withTitle ("Clear all MIDI mappings?")
                       .withMessage ("Every CC mapping in this instance is removed. The saved default map is not changed.")
                       .withButton ("Clear all")
                       .withButton ("Cancel")
                       .withAssociatedComponent (this);

    juce::AlertWindow::showAsync (options, [safe = juce::Component::SafePointer<SettingsDialog> (this)] (int result)
                                  {
                                      if (safe != nullptr && result == 1)
                                          safe->processor.clearMidiMap();
                                  });
}
} // namespace p5x::standalone

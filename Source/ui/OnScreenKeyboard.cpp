// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#include "ui/OnScreenKeyboard.h"
#include "PluginProcessor.h"
#include "ui/LookAndFeelP5X.h"

namespace p5x::ui
{
class OnScreenKeyboard::KeyboardView : public juce::MidiKeyboardComponent
{
public:
    KeyboardView (juce::MidiKeyboardState& s, P5XAudioProcessor& p)
        : MidiKeyboardComponent (s, horizontalKeyboard), processor (p)
    {
        setAvailableRange (midi::MidiHandler::kLowestNote, midi::MidiHandler::kHighestNote);
        setScrollButtonsVisible (false);
        setOctaveForMiddleC (4); // MIDI 60 = C4, as in the docs
        clearKeyMappings();      // the Standalone computer keyboard has its own mapping (10-debug-and-harness.md)
        setWantsKeyboardFocus (false);
        setVelocity (1.0f, true);
    }

    void drawWhiteNote (int note, juce::Graphics& g, juce::Rectangle<float> area, bool isDown, bool isOver,
                        juce::Colour lineColour, juce::Colour textColour) override
    {
        MidiKeyboardComponent::drawWhiteNote (note, g, area, isDown || processor.isNoteHeld (note), isOver,
                                              lineColour, textColour);
    }

    void drawBlackNote (int note, juce::Graphics& g, juce::Rectangle<float> area, bool isDown, bool isOver,
                        juce::Colour fill) override
    {
        MidiKeyboardComponent::drawBlackNote (note, g, area, isDown || processor.isNoteHeld (note), isOver, fill);
    }

private:
    P5XAudioProcessor& processor;
};

OnScreenKeyboard::OnScreenKeyboard (P5XAudioProcessor& p)
    : processor (p)
{
    keyboard = std::make_unique<KeyboardView> (state, processor);
    keyboard->setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, colours::accent);
    keyboard->setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, colours::accent.withAlpha (0.25f));
    addAndMakeVisible (*keyboard);
    state.addListener (this);

    for (auto* wheel : { &pitchWheel, &modWheel })
    {
        wheel->setSliderStyle (juce::Slider::LinearBarVertical);
        wheel->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        wheel->setColour (juce::Slider::trackColourId, colours::accent.withAlpha (0.6f));
        addAndMakeVisible (*wheel);
    }

    pitchWheel.setTitle ("Pitch wheel");
    pitchWheel.setRange (-8192.0, 8191.0, 1.0);
    pitchWheel.setValue (0.0, juce::dontSendNotification);
    pitchWheel.onValueChange = [this]
    {
        const int value14 = juce::jlimit (0, 16383, (int) pitchWheel.getValue() + 8192);
        processor.injectMidi (juce::MidiMessage::pitchWheel (juce::jmax (1, processor.getMidiChannel()), value14));
    };
    pitchWheel.onDragEnd = [this] { pitchWheel.setValue (0.0); }; // springs back

    modWheel.setTitle ("Mod wheel");
    modWheel.setRange (0.0, 127.0, 1.0);
    modWheel.onValueChange = [this]
    {
        processor.injectMidi (juce::MidiMessage::controllerEvent (juce::jmax (1, processor.getMidiChannel()), 1,
                                                                  (int) modWheel.getValue()));
    };

    startTimerHz (30);
}

OnScreenKeyboard::~OnScreenKeyboard()
{
    state.removeListener (this);
}

void OnScreenKeyboard::paint (juce::Graphics& g)
{
    g.fillAll (colours::topBar);
    g.setColour (colours::labelDim);
    g.setFont (LookAndFeelP5X::font (11.0f));

    const auto labels = getLocalBounds().removeFromBottom (14).removeFromLeft (76);
    g.drawText ("PITCH", labels.withWidth (38), juce::Justification::centred);
    g.drawText ("MOD", labels.withTrimmedLeft (38), juce::Justification::centred);
}

void OnScreenKeyboard::resized()
{
    auto area = getLocalBounds().reduced (4);
    auto wheels = area.removeFromLeft (76);
    wheels.removeFromBottom (12);
    pitchWheel.setBounds (wheels.removeFromLeft (38).reduced (6, 0));
    modWheel.setBounds (wheels.reduced (6, 0));

    keyboard->setBounds (area);
    // 36 white keys fill the width.
    keyboard->setKeyWidth ((float) area.getWidth() / 36.0f);
}

void OnScreenKeyboard::handleNoteOn (juce::MidiKeyboardState*, int, int note, float velocity)
{
    processor.injectNoteOn (note, juce::jlimit (1, 127, juce::roundToInt (velocity * 127.0f)));
}

void OnScreenKeyboard::handleNoteOff (juce::MidiKeyboardState*, int, int note, float)
{
    processor.injectNoteOff (note);
}

void OnScreenKeyboard::timerCallback()
{
    std::array<uint64_t, 2> held {};

    for (int note = 0; note < 128; ++note)
        if (processor.isNoteHeld (note))
            held[(size_t) (note >> 6)] |= 1ull << (note & 63);

    if (held != lastHeld)
    {
        lastHeld = held;
        keyboard->repaint();
    }
}
} // namespace p5x::ui

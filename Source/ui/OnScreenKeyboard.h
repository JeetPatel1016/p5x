// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

class P5XAudioProcessor;

namespace p5x::ui
{
// 61 keys, C2–C7 (MIDI 36–96), with pitch and mod wheels on the left (08-ui.md § Components).
// Clicks are sent to the processor through its lock-free injection FIFO; keys held on any source
// (KeyLab, computer keyboard, mouse) are tinted accent.
class OnScreenKeyboard : public juce::Component,
                         private juce::MidiKeyboardState::Listener,
                         private juce::Timer
{
public:
    explicit OnScreenKeyboard (P5XAudioProcessor& processor);
    ~OnScreenKeyboard() override;

    void resized() override;
    void paint (juce::Graphics& g) override;

private:
    class KeyboardView;

    void handleNoteOn (juce::MidiKeyboardState*, int channel, int note, float velocity) override;
    void handleNoteOff (juce::MidiKeyboardState*, int channel, int note, float velocity) override;
    void timerCallback() override;

    P5XAudioProcessor& processor;
    juce::MidiKeyboardState state;
    std::unique_ptr<KeyboardView> keyboard;
    juce::Slider pitchWheel, modWheel;
    std::array<uint64_t, 2> lastHeld {};
};
} // namespace p5x::ui

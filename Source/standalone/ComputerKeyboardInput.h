// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#pragma once

#include <juce_core/juce_core.h>

#include <map>
#include <optional>
#include <vector>

namespace p5x::standalone
{
// Computer keyboard → notes (10-debug-and-harness.md § Standalone test harness):
// A S D F G H J K = C D E F G A B C, W E T Y U = C# D# F# G# A#, Z / X = octave down / up.
// Pure logic, so it's unit-testable; the Standalone window feeds it key events.
class ComputerKeyboardInput
{
public:
    static constexpr int kVelocity = 100;
    static constexpr int kDefaultBaseNote = 48; // the A key plays C3
    static constexpr int kLowestBaseNote = 36;  // C2: every key stays inside MIDI 36–96
    static constexpr int kHighestBaseNote = 84; // C6

    struct NoteEvent
    {
        int note = 0;
        bool on = false;
    };

    // Returns the note to start, or nothing (unmapped key, octave key, or key repeat).
    std::optional<NoteEvent> keyDown (juce::juce_wchar key);
    // Returns the note to stop if this key was holding one.
    std::optional<NoteEvent> keyUp (juce::juce_wchar key);

    // Keys currently holding a note (the window polls these for release).
    std::vector<juce::juce_wchar> getHeldKeys() const;
    // Note offs for everything held (focus lost, disabled).
    std::vector<NoteEvent> releaseAll();

    int getBaseNote() const noexcept { return baseNote; }

    // Semitone offset from the base C for a note key, or −1.
    static int offsetForKey (juce::juce_wchar key) noexcept;

private:
    int baseNote = kDefaultBaseNote;
    std::map<juce::juce_wchar, int> held; // key → sounding note
};
} // namespace p5x::standalone

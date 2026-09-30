// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#pragma once

#include <cstdint>

namespace p5x::midi
{
// Receives the decoded MIDI table from 07-midi.md. Called on the audio thread.
struct MidiSink
{
    virtual ~MidiSink() = default;

    virtual void noteOn (int note, int velocity) = 0;
    virtual void noteOff (int note) = 0;
    virtual void sustainPedal (bool down) = 0;
    virtual void allSoundOff() = 0;
    virtual void allNotesOff() = 0;
    virtual void resetAllControllers() = 0;
    virtual void pitchBend (int value14) = 0;
    virtual void modWheel (int value) = 0;
    virtual void channelPressure (int value) = 0;
    virtual void polyPressure (int note, int value) = 0;
    // Every CC that passes the channel filter, reserved ones included, so MIDI Learn can warn about them.
    virtual void controlChange (int cc, int value) = 0;
    virtual void programChange (int program) = 0;
};

// Decodes raw MIDI bytes (never juce::MidiMessage, which can allocate for long messages) and applies
// the channel filter and the 61-key note range.
class MidiHandler
{
public:
    static constexpr int kLowestNote = 36;  // C2, see 07-midi.md § Note range
    static constexpr int kHighestNote = 96; // C7

    enum class Result
    {
        Handled,
        WrongChannel, // dropped by the channel filter (shown dimmed in the MIDI monitor)
        OutOfRange,   // note outside 36–96 (shown dimmed in the MIDI monitor)
        Ignored       // SysEx, clock, malformed, etc.
    };

    // 0 = Omni, 1–16 = that channel only.
    void setChannel (int newChannel) noexcept { channel = newChannel; }
    int getChannel() const noexcept { return channel; }

    Result handle (const uint8_t* data, int size, MidiSink& sink) const noexcept;

    static constexpr bool isNoteInRange (int note) noexcept { return note >= kLowestNote && note <= kHighestNote; }

private:
    int channel = 0;
};
} // namespace p5x::midi

// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#pragma once

#include <juce_core/juce_core.h>

#include <array>
#include <cstdint>

namespace p5x::midi
{
// One incoming message, captured before the channel filter (10-debug-and-harness.md § MIDI monitor).
struct MonitorEvent
{
    double timeMs = 0.0;
    uint8_t bytes[3] {};
    uint8_t size = 0;
    bool dimmed = false; // filtered by channel or outside the note range
};

// Audio thread → message thread (SPSC; one audio thread per plugin instance).
class MidiMonitorFifo
{
public:
    static constexpr int kCapacity = 1024;

    bool push (const MonitorEvent& event) noexcept
    {
        const auto scope = fifo.write (1);

        if (scope.blockSize1 == 0)
            return false;

        buffer[(size_t) scope.startIndex1] = event;
        return true;
    }

    bool pop (MonitorEvent& event) noexcept
    {
        const auto scope = fifo.read (1);

        if (scope.blockSize1 == 0)
            return false;

        event = buffer[(size_t) scope.startIndex1];
        return true;
    }

private:
    juce::AbstractFifo fifo { kCapacity };
    std::array<MonitorEvent, kCapacity> buffer {};
};

// "Note On", "CC", ... for the monitor's type column.
inline const char* describeType (const MonitorEvent& e) noexcept
{
    if (e.size == 0)
        return "?";

    const int status = e.bytes[0];

    if (status >= 0xF0)
        return "System";

    switch (status & 0xF0)
    {
        case 0x80: return "Note Off";
        case 0x90: return (e.size > 2 && e.bytes[2] == 0) ? "Note Off" : "Note On";
        case 0xA0: return "Poly Pressure";
        case 0xB0: return "CC";
        case 0xC0: return "Program";
        case 0xD0: return "Ch Pressure";
        case 0xE0: return "Pitch Bend";
        default:   return "?";
    }
}
} // namespace p5x::midi

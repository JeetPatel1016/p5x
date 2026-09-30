// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#include "midi/MidiHandler.h"

namespace p5x::midi
{
MidiHandler::Result MidiHandler::handle (const uint8_t* data, int size, MidiSink& sink) const noexcept
{
    if (data == nullptr || size < 1)
        return Result::Ignored;

    const int status = data[0];

    // Data bytes without status (no running status inside a MidiBuffer) and all system messages.
    if (status < 0x80 || status >= 0xF0)
        return Result::Ignored;

    const int type = status & 0xF0;
    const int messageChannel = (status & 0x0F) + 1;
    const int needed = (type == 0xC0 || type == 0xD0) ? 2 : 3;

    if (size < needed)
        return Result::Ignored;

    if (channel != 0 && messageChannel != channel)
        return Result::WrongChannel;

    const int d1 = data[1] & 0x7F;
    const int d2 = needed == 3 ? (data[2] & 0x7F) : 0;

    switch (type)
    {
        case 0x90:
            if (! isNoteInRange (d1))
                return Result::OutOfRange;

            if (d2 == 0)
                sink.noteOff (d1);
            else
                sink.noteOn (d1, d2);

            return Result::Handled;

        case 0x80:
            if (! isNoteInRange (d1))
                return Result::OutOfRange;

            sink.noteOff (d1);
            return Result::Handled;

        case 0xA0:
            if (! isNoteInRange (d1))
                return Result::OutOfRange;

            sink.polyPressure (d1, d2);
            return Result::Handled;

        case 0xB0:
            sink.controlChange (d1, d2);

            switch (d1)
            {
                case 1:   sink.modWheel (d2); break;
                case 64:  sink.sustainPedal (d2 >= 64); break;
                case 120: sink.allSoundOff(); break;
                case 121: sink.resetAllControllers(); break;
                case 123: sink.allNotesOff(); break;
                default:  break; // 122, 124–127 ignored; others go to MIDI Learn via controlChange
            }

            return Result::Handled;

        case 0xC0:
            sink.programChange (d1);
            return Result::Handled;

        case 0xD0:
            sink.channelPressure (d1);
            return Result::Handled;

        case 0xE0:
            sink.pitchBend (d1 | (d2 << 7));
            return Result::Handled;

        default:
            return Result::Ignored;
    }
}
} // namespace p5x::midi

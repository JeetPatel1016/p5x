// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#include "midi/MidiHandler.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

using p5x::midi::MidiHandler;
using Result = MidiHandler::Result;

namespace
{
struct RecordingSink : p5x::midi::MidiSink
{
    std::vector<std::string> events;

    void add (const std::string& name, int a = -1, int b = -1)
    {
        std::string s = name;

        if (a >= 0)
            s += " " + std::to_string (a);

        if (b >= 0)
            s += " " + std::to_string (b);

        events.push_back (s);
    }

    void noteOn (int note, int velocity) override { add ("on", note, velocity); }
    void noteOff (int note) override { add ("off", note); }
    void sustainPedal (bool down) override { add (down ? "pedal down" : "pedal up"); }
    void allSoundOff() override { add ("all sound off"); }
    void allNotesOff() override { add ("all notes off"); }
    void resetAllControllers() override { add ("reset controllers"); }
    void pitchBend (int value14) override { add ("bend", value14); }
    void modWheel (int value) override { add ("wheel", value); }
    void channelPressure (int value) override { add ("pressure", value); }
    void polyPressure (int note, int value) override { add ("poly pressure", note, value); }
    void controlChange (int cc, int value) override { add ("cc", cc, value); }
    void programChange (int program) override { add ("program", program); }
};

Result send (MidiHandler& h, RecordingSink& sink, std::vector<uint8_t> bytes)
{
    return h.handle (bytes.data(), (int) bytes.size(), sink);
}
} // namespace

TEST_CASE ("MidiHandler: notes, and Note On with velocity 0 is a Note Off", "[midi]")
{
    MidiHandler h;
    RecordingSink sink;

    REQUIRE (send (h, sink, { 0x90, 60, 100 }) == Result::Handled);
    REQUIRE (send (h, sink, { 0x90, 60, 0 }) == Result::Handled);
    REQUIRE (send (h, sink, { 0x80, 62, 64 }) == Result::Handled);

    REQUIRE (sink.events == std::vector<std::string> { "on 60 100", "off 60", "off 62" });
}

TEST_CASE ("MidiHandler: channel filter 3 ignores channel 1", "[midi]")
{
    MidiHandler h;
    RecordingSink sink;
    h.setChannel (3);

    REQUIRE (send (h, sink, { 0x90, 60, 100 }) == Result::WrongChannel); // channel 1
    REQUIRE (send (h, sink, { 0xB0, 1, 64 }) == Result::WrongChannel);
    REQUIRE (sink.events.empty());

    REQUIRE (send (h, sink, { 0x92, 60, 100 }) == Result::Handled); // channel 3
    REQUIRE (sink.events == std::vector<std::string> { "on 60 100" });
}

TEST_CASE ("MidiHandler: Omni accepts every channel", "[midi]")
{
    MidiHandler h;
    RecordingSink sink;

    for (uint8_t ch = 0; ch < 16; ++ch)
        REQUIRE (send (h, sink, { (uint8_t) (0x90 | ch), 60, 1 }) == Result::Handled);

    REQUIRE (sink.events.size() == 16);
}

TEST_CASE ("MidiHandler: 61-key range, notes 36 to 96", "[midi]")
{
    MidiHandler h;
    RecordingSink sink;

    REQUIRE (send (h, sink, { 0x90, 35, 100 }) == Result::OutOfRange);
    REQUIRE (send (h, sink, { 0x90, 97, 100 }) == Result::OutOfRange);
    REQUIRE (send (h, sink, { 0x80, 97, 0 }) == Result::OutOfRange);
    REQUIRE (send (h, sink, { 0xA0, 20, 50 }) == Result::OutOfRange);
    REQUIRE (sink.events.empty());

    REQUIRE (send (h, sink, { 0x90, 36, 100 }) == Result::Handled);
    REQUIRE (send (h, sink, { 0x90, 96, 100 }) == Result::Handled);
    REQUIRE (sink.events == std::vector<std::string> { "on 36 100", "on 96 100" });
}

TEST_CASE ("MidiHandler: hardwired CCs, and every CC reaches MIDI Learn", "[midi]")
{
    MidiHandler h;
    RecordingSink sink;

    send (h, sink, { 0xB0, 1, 90 });
    send (h, sink, { 0xB0, 64, 64 });
    send (h, sink, { 0xB0, 64, 63 });
    send (h, sink, { 0xB0, 120, 0 });
    send (h, sink, { 0xB0, 121, 0 });
    send (h, sink, { 0xB0, 123, 0 });
    send (h, sink, { 0xB0, 74, 33 });

    REQUIRE (sink.events == std::vector<std::string> {
                                "cc 1 90", "wheel 90",
                                "cc 64 64", "pedal down",
                                "cc 64 63", "pedal up",
                                "cc 120 0", "all sound off",
                                "cc 121 0", "reset controllers",
                                "cc 123 0", "all notes off",
                                "cc 74 33" });
}

TEST_CASE ("MidiHandler: CC 122 and 124-127 have no function", "[midi]")
{
    MidiHandler h;
    RecordingSink sink;

    for (uint8_t cc : { 122, 124, 125, 126, 127 })
        send (h, sink, { 0xB0, cc, 0 });

    // Only the MIDI Learn path sees them (which ignores reserved CCs).
    REQUIRE (sink.events.size() == 5);

    for (const auto& e : sink.events)
        REQUIRE (e.rfind ("cc ", 0) == 0);
}

TEST_CASE ("MidiHandler: bend, pressure, program change", "[midi]")
{
    MidiHandler h;
    RecordingSink sink;

    send (h, sink, { 0xE0, 0x00, 0x40 }); // centre 8192
    send (h, sink, { 0xE0, 0x7F, 0x7F }); // 16383
    send (h, sink, { 0xD0, 77 });
    send (h, sink, { 0xA0, 60, 12 });
    send (h, sink, { 0xC0, 5 });

    REQUIRE (sink.events == std::vector<std::string> { "bend 8192", "bend 16383", "pressure 77", "poly pressure 60 12",
                                                       "program 5" });
}

TEST_CASE ("MidiHandler: system and malformed messages are ignored", "[midi]")
{
    MidiHandler h;
    RecordingSink sink;

    REQUIRE (send (h, sink, { 0xF0, 0x7E, 0x7F, 0x09, 0x01, 0xF7 }) == Result::Ignored); // SysEx
    REQUIRE (send (h, sink, { 0xF8 }) == Result::Ignored);                               // clock
    REQUIRE (send (h, sink, { 0x40, 0x10 }) == Result::Ignored);                         // no status
    REQUIRE (send (h, sink, { 0x90, 60 }) == Result::Ignored);                           // truncated
    REQUIRE (h.handle (nullptr, 0, sink) == Result::Ignored);
    REQUIRE (sink.events.empty());
}

TEST_CASE ("MidiHandler: data bytes are masked to 7 bits", "[midi]")
{
    MidiHandler h;
    RecordingSink sink;

    send (h, sink, { 0x90, 60, 0xFF });
    REQUIRE (sink.events == std::vector<std::string> { "on 60 127" });
}

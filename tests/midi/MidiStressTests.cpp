// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
//
// 07-midi.md § Tests: rapid note on/off + sustain sequences (10 000 random events) leave no stuck
// voices after All Notes Off + pedal up. Runs the real MidiHandler and VoiceAllocator together.

#include "dsp/Random.h"
#include "dsp/VoiceAllocator.h"
#include "midi/MidiHandler.h"

#include <catch2/catch_test_macros.hpp>

#include <array>

namespace
{
struct FakeVoice
{
    bool idle = true, releasing = false;

    bool isIdle() const { return idle; }
    void start (int, float) { idle = false; releasing = false; }
    void release() { if (! idle) releasing = true; }
    void kill() { idle = true; releasing = false; }
};

struct AllocatorSink : p5x::midi::MidiSink
{
    std::array<FakeVoice, 10> voices;
    p5x::dsp::VoiceAllocator<FakeVoice, 10> allocator { voices };

    void noteOn (int note, int velocity) override { allocator.noteOn (note, (float) velocity / 127.0f); }
    void noteOff (int note) override { allocator.noteOff (note); }
    void sustainPedal (bool down) override { allocator.setSustainPedal (down); }
    void allSoundOff() override { allocator.allSoundOff(); }
    void allNotesOff() override { allocator.allNotesOff(); }
    void resetAllControllers() override { allocator.setSustainPedal (false); }
    void pitchBend (int) override {}
    void modWheel (int) override {}
    void channelPressure (int) override {}
    void polyPressure (int, int) override {}
    void controlChange (int, int) override {}
    void programChange (int) override {}
};
} // namespace

TEST_CASE ("MIDI stress: 10 000 random events, then All Notes Off + pedal up: nothing stuck", "[midi][stress]")
{
    for (uint64_t seed : { 1ull, 2ull, 3ull })
    {
        p5x::midi::MidiHandler handler;
        AllocatorSink sink;
        sink.allocator.setActiveVoiceCount (5);
        p5x::Random random (seed);

        for (int i = 0; i < 10000; ++i)
        {
            const int kind = (int) (random.nextDouble() * 10.0);
            const auto note = (uint8_t) (30 + (int) (random.nextDouble() * 72.0)); // includes out-of-range notes
            std::array<uint8_t, 3> bytes {};

            if (kind < 4)
                bytes = { 0x90, note, (uint8_t) (1 + (int) (random.nextDouble() * 126.0)) };
            else if (kind < 8)
                bytes = { (uint8_t) (random.nextDouble() < 0.5 ? 0x80 : 0x90), note, 0 };
            else
                bytes = { 0xB0, 64, (uint8_t) (random.nextDouble() < 0.5 ? 0 : 127) };

            handler.handle (bytes.data(), 3, sink);
        }

        const std::array<uint8_t, 3> allNotesOff { 0xB0, 123, 0 }, pedalUp { 0xB0, 64, 0 };
        handler.handle (allNotesOff.data(), 3, sink);
        handler.handle (pedalUp.data(), 3, sink);

        for (int v = 0; v < 10; ++v)
        {
            INFO ("seed " << seed << ", voice " << v);
            REQUIRE ((sink.voices[(size_t) v].idle || sink.voices[(size_t) v].releasing));
            REQUIRE_FALSE (sink.allocator.isKeyDown (v));
            REQUIRE_FALSE (sink.allocator.isSustained (v));
        }

        REQUIRE_FALSE (sink.allocator.isSustainPedalDown());
    }
}

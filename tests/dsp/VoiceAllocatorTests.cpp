// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#include "dsp/VoiceAllocator.h"

#include <catch2/catch_test_macros.hpp>

#include <array>

namespace
{
// Stands in for a voice: releasing voices stay non-idle until finish() is called.
struct FakeVoice
{
    bool idle = true, releasing = false;
    int note = -1, starts = 0;

    bool isIdle() const { return idle; }
    void start (int n, float) { idle = false; releasing = false; note = n; ++starts; }
    void release() { if (! idle) releasing = true; }
    void kill() { idle = true; releasing = false; }
    void finish() { if (releasing) kill(); }
};

struct Fixture
{
    std::array<FakeVoice, 10> voices;
    p5x::dsp::VoiceAllocator<FakeVoice, 10> allocator { voices };

    Fixture() { allocator.setActiveVoiceCount (5); }
};
} // namespace

TEST_CASE ("Allocator: 5 voices, 6 held notes: the 6th steals the oldest held", "[dsp][allocator]")
{
    Fixture f;

    for (int n = 60; n < 65; ++n)
        REQUIRE_FALSE (f.allocator.noteOn (n, 1.0f).stolen);

    const auto result = f.allocator.noteOn (65, 1.0f);
    REQUIRE (result.stolen);
    REQUIRE (result.stolenNote == 60);
    REQUIRE (f.voices[(size_t) result.voice].note == 65);
    REQUIRE (f.allocator.findVoicePlayingNote (60) < 0);
}

TEST_CASE ("Allocator: same note twice uses one voice", "[dsp][allocator]")
{
    Fixture f;
    const auto first = f.allocator.noteOn (60, 1.0f);
    const auto second = f.allocator.noteOn (60, 1.0f);

    REQUIRE (first.voice == second.voice);
    REQUIRE_FALSE (second.stolen);
    REQUIRE (f.voices[(size_t) first.voice].starts == 2);

    int sounding = 0;

    for (const auto& v : f.voices)
        sounding += v.isIdle() ? 0 : 1;

    REQUIRE (sounding == 1);
}

TEST_CASE ("Allocator: a releasing voice with the same note is reused", "[dsp][allocator]")
{
    Fixture f;
    const auto first = f.allocator.noteOn (60, 1.0f);
    f.allocator.noteOff (60);
    const auto again = f.allocator.noteOn (60, 1.0f);

    REQUIRE (again.voice == first.voice);
}

TEST_CASE ("Allocator: idle voices are assigned round-robin", "[dsp][allocator]")
{
    Fixture f;
    const auto a = f.allocator.noteOn (60, 1.0f);
    f.allocator.noteOff (60);
    f.voices[(size_t) a.voice].finish();

    const auto b = f.allocator.noteOn (62, 1.0f);
    REQUIRE (b.voice == (a.voice + 1) % 5);

    const auto c = f.allocator.noteOn (64, 1.0f);
    REQUIRE (c.voice == (b.voice + 1) % 5);
}

TEST_CASE ("Allocator: steal order is releasing, then pedal-held, then key-held", "[dsp][allocator]")
{
    Fixture f;

    for (int n = 60; n < 65; ++n)
        f.allocator.noteOn (n, 1.0f);

    // 60 and 61 held only by the pedal, 63 releasing, 62/64 still held by keys.
    f.allocator.setSustainPedal (true);
    f.allocator.noteOff (60);
    f.allocator.noteOff (61);
    f.allocator.setSustainPedal (false); // releases 60 and 61
    f.allocator.setSustainPedal (true);
    f.allocator.noteOn (60, 1.0f);       // reuses 60's releasing voice, key-held again
    f.allocator.noteOff (60);            // 60 now pedal-held (newest)
    f.allocator.noteOff (63);            // pedal down: 63 pedal-held too

    // Releasing: 61 only.
    auto r = f.allocator.noteOn (70, 1.0f);
    REQUIRE (r.stolen);
    REQUIRE (r.stolenNote == 61);

    // No releasing voices left: oldest pedal-held is 63 (60 was re-struck later).
    r = f.allocator.noteOn (71, 1.0f);
    REQUIRE (r.stolenNote == 63);

    r = f.allocator.noteOn (72, 1.0f);
    REQUIRE (r.stolenNote == 60);

    // Only key-held voices remain: oldest is 62.
    r = f.allocator.noteOn (73, 1.0f);
    REQUIRE (r.stolenNote == 62);
}

TEST_CASE ("Allocator: sustain pedal holds released keys until pedal up", "[dsp][allocator]")
{
    Fixture f;
    const auto v = f.allocator.noteOn (60, 1.0f).voice;

    f.allocator.setSustainPedal (true);
    f.allocator.noteOff (60);
    REQUIRE_FALSE (f.voices[(size_t) v].releasing);
    REQUIRE (f.allocator.isSustained (v));

    f.allocator.setSustainPedal (false);
    REQUIRE (f.voices[(size_t) v].releasing);
    REQUIRE_FALSE (f.allocator.isSustained (v));
}

TEST_CASE ("Allocator: re-pressing a sustained note reuses its voice", "[dsp][allocator]")
{
    Fixture f;
    const auto v = f.allocator.noteOn (60, 1.0f).voice;
    f.allocator.setSustainPedal (true);
    f.allocator.noteOff (60);

    const auto again = f.allocator.noteOn (60, 1.0f);
    REQUIRE (again.voice == v);
    REQUIRE (f.allocator.isKeyDown (v));
    REQUIRE_FALSE (f.allocator.isSustained (v));
}

TEST_CASE ("Allocator: All Notes Off releases everything and clears the pedal", "[dsp][allocator]")
{
    Fixture f;
    f.allocator.setSustainPedal (true);

    for (int n = 60; n < 63; ++n)
        f.allocator.noteOn (n, 1.0f);

    f.allocator.noteOff (60);
    f.allocator.allNotesOff();

    REQUIRE_FALSE (f.allocator.isSustainPedalDown());

    for (int i = 0; i < 3; ++i)
    {
        REQUIRE (f.voices[(size_t) i].releasing);
        REQUIRE_FALSE (f.allocator.isKeyDown (i));
        REQUIRE_FALSE (f.allocator.isSustained (i));
    }
}

TEST_CASE ("Allocator: All Sound Off cuts every voice", "[dsp][allocator]")
{
    Fixture f;

    for (int n = 60; n < 65; ++n)
        f.allocator.noteOn (n, 1.0f);

    f.allocator.allSoundOff();
    REQUIRE (f.allocator.allVoicesIdle());
}

TEST_CASE ("Allocator: the active count limits the voices used", "[dsp][allocator]")
{
    Fixture f;
    f.allocator.setActiveVoiceCount (8);

    for (int n = 60; n < 68; ++n)
        REQUIRE_FALSE (f.allocator.noteOn (n, 1.0f).stolen);

    REQUIRE (f.allocator.noteOn (70, 1.0f).stolen);

    for (size_t i = 8; i < 10; ++i)
        REQUIRE (f.voices[i].isIdle());
}

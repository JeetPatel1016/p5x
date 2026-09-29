// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#pragma once

#include <algorithm>
#include <array>
#include <cstdint>

namespace p5x::dsp
{
// Poly voice assignment, sustain pedal and stealing (06-voices.md § Allocation, § Sustain pedal).
// VoiceType needs: isIdle(), start (note, velocity01), release(), kill().
// Never logs (dsp/ is standard-library only); steals are reported in NoteOnResult.
template <typename VoiceType, int MaxVoices>
class VoiceAllocator
{
public:
    struct NoteOnResult
    {
        int voice = -1;
        bool stolen = false;
        int stolenNote = -1;
    };

    explicit VoiceAllocator (std::array<VoiceType, MaxVoices>& voicesToUse) noexcept : voices (voicesToUse) {}

    void reset() noexcept
    {
        slots = {};
        ageCounter = 0;
        lastAssigned = -1;
        pedalDown = false;
    }

    void setActiveVoiceCount (int count) noexcept { activeCount = std::clamp (count, 1, MaxVoices); }
    int getActiveVoiceCount() const noexcept { return activeCount; }

    NoteOnResult noteOn (int note, float velocity01) noexcept
    {
        NoteOnResult result;

        // 1. Same note already sounding (gate on or releasing): reuse that voice.
        result.voice = findVoicePlayingNote (note);

        // 2. Next idle voice, round-robin from after the last one assigned.
        if (result.voice < 0)
        {
            for (int k = 1; k <= activeCount; ++k)
            {
                const int i = (lastAssigned + k + MaxVoices) % activeCount;

                if (voices[(size_t) i].isIdle())
                {
                    result.voice = i;
                    break;
                }
            }
        }

        // 3. Steal: oldest releasing, else oldest held only by the pedal, else oldest key-held.
        if (result.voice < 0)
        {
            result.voice = oldestMatching ([this] (int i) { return ! slots[(size_t) i].keyDown && ! slots[(size_t) i].sustained; });

            if (result.voice < 0)
                result.voice = oldestMatching ([this] (int i) { return slots[(size_t) i].sustained && ! slots[(size_t) i].keyDown; });

            if (result.voice < 0)
                result.voice = oldestMatching ([] (int) { return true; });

            result.stolen = true;
            result.stolenNote = slots[(size_t) result.voice].note;
        }

        auto& slot = slots[(size_t) result.voice];
        slot.note = note;
        slot.keyDown = true;
        slot.sustained = false;
        slot.age = ++ageCounter;
        lastAssigned = result.voice;

        voices[(size_t) result.voice].start (note, velocity01);
        return result;
    }

    void noteOff (int note) noexcept
    {
        for (int i = 0; i < activeCount; ++i)
        {
            auto& slot = slots[(size_t) i];

            if (slot.keyDown && slot.note == note)
            {
                slot.keyDown = false;

                if (pedalDown)
                    slot.sustained = true;
                else
                    voices[(size_t) i].release();
            }
        }
    }

    void setSustainPedal (bool down) noexcept
    {
        pedalDown = down;

        if (down)
            return;

        for (int i = 0; i < MaxVoices; ++i)
        {
            if (slots[(size_t) i].sustained)
            {
                slots[(size_t) i].sustained = false;
                voices[(size_t) i].release();
            }
        }
    }

    bool isSustainPedalDown() const noexcept { return pedalDown; }

    // Forgets the pedal without releasing anything (used by Panic after allSoundOff).
    void clearSustainState() noexcept { pedalDown = false; }

    // CC 123: every voice to Release; sustain state cleared (07-midi.md).
    void allNotesOff() noexcept
    {
        for (int i = 0; i < MaxVoices; ++i)
        {
            slots[(size_t) i].keyDown = false;
            slots[(size_t) i].sustained = false;
            voices[(size_t) i].release();
        }

        pedalDown = false;
    }

    // CC 120: hard cut to Idle.
    void allSoundOff() noexcept
    {
        for (int i = 0; i < MaxVoices; ++i)
        {
            slots[(size_t) i].keyDown = false;
            slots[(size_t) i].sustained = false;
            voices[(size_t) i].kill();
        }
    }

    // The voice currently sounding `note` (gate on, sustained or releasing), or -1.
    int findVoicePlayingNote (int note) const noexcept
    {
        for (int i = 0; i < activeCount; ++i)
            if (slots[(size_t) i].note == note && ! voices[(size_t) i].isIdle())
                return i;

        return -1;
    }

    bool allVoicesIdle() const noexcept
    {
        return std::all_of (voices.begin(), voices.end(), [] (const VoiceType& v) { return v.isIdle(); });
    }

    bool isKeyDown (int voice) const noexcept { return slots[(size_t) voice].keyDown; }
    bool isSustained (int voice) const noexcept { return slots[(size_t) voice].sustained; }
    int getNote (int voice) const noexcept { return slots[(size_t) voice].note; }

private:
    struct Slot
    {
        int note = -1;
        bool keyDown = false;
        bool sustained = false;
        uint64_t age = 0;
    };

    template <typename Predicate>
    int oldestMatching (Predicate&& predicate) const noexcept
    {
        int best = -1;

        for (int i = 0; i < activeCount; ++i)
        {
            if (voices[(size_t) i].isIdle() || ! predicate (i))
                continue;

            if (best < 0 || slots[(size_t) i].age < slots[(size_t) best].age)
                best = i;
        }

        return best;
    }

    std::array<VoiceType, MaxVoices>& voices;
    std::array<Slot, MaxVoices> slots {};
    uint64_t ageCounter = 0;
    int activeCount = MaxVoices;
    int lastAssigned = -1;
    bool pedalDown = false;
};
} // namespace p5x::dsp

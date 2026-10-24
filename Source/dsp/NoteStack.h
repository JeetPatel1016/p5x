// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#pragma once

#include <array>

namespace p5x::dsp
{
// Unison note stack (06-voices.md § Unison): last-note priority over up to 16 notes, dropping the
// oldest when full. With the sustain pedal down a released key stays in the stack; on pedal up the
// notes whose keys are up leave it.
class NoteStack
{
public:
    static constexpr int kCapacity = 16;

    void clear() noexcept { count = 0; }

    // A new note goes on top (moved there if it was already in the stack).
    void push (int note) noexcept
    {
        remove (note);

        if (count == kCapacity)
            removeAt (0); // drop the oldest

        entries[(size_t) count++] = { note, true };
    }

    // Key released: with the pedal down the note stays (sustained), otherwise it leaves.
    void keyUp (int note, bool pedalDown) noexcept
    {
        for (int i = 0; i < count; ++i)
        {
            if (entries[(size_t) i].note == note)
            {
                if (pedalDown)
                    entries[(size_t) i].keyDown = false;
                else
                    removeAt (i);

                return;
            }
        }
    }

    // Pedal up: every note whose key is up leaves the stack.
    void pedalUp() noexcept
    {
        for (int i = count - 1; i >= 0; --i)
            if (! entries[(size_t) i].keyDown)
                removeAt (i);
    }

    int top() const noexcept { return count > 0 ? entries[(size_t) count - 1].note : -1; }
    int size() const noexcept { return count; }
    bool contains (int note) const noexcept
    {
        for (int i = 0; i < count; ++i)
            if (entries[(size_t) i].note == note)
                return true;

        return false;
    }

private:
    struct Entry
    {
        int note = -1;
        bool keyDown = false;
    };

    void remove (int note) noexcept
    {
        for (int i = 0; i < count; ++i)
            if (entries[(size_t) i].note == note)
            {
                removeAt (i);
                return;
            }
    }

    void removeAt (int index) noexcept
    {
        for (int i = index; i < count - 1; ++i)
            entries[(size_t) i] = entries[(size_t) i + 1];

        --count;
    }

    std::array<Entry, kCapacity> entries {};
    int count = 0;
};
} // namespace p5x::dsp

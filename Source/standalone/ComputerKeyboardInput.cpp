// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#include "standalone/ComputerKeyboardInput.h"

namespace p5x::standalone
{
int ComputerKeyboardInput::offsetForKey (juce::juce_wchar key) noexcept
{
    switch (juce::CharacterFunctions::toLowerCase (key))
    {
        case 'a': return 0;
        case 'w': return 1;
        case 's': return 2;
        case 'e': return 3;
        case 'd': return 4;
        case 'f': return 5;
        case 't': return 6;
        case 'g': return 7;
        case 'y': return 8;
        case 'h': return 9;
        case 'u': return 10;
        case 'j': return 11;
        case 'k': return 12;
        default:  return -1;
    }
}

std::optional<ComputerKeyboardInput::NoteEvent> ComputerKeyboardInput::keyDown (juce::juce_wchar key)
{
    const auto lower = juce::CharacterFunctions::toLowerCase (key);

    if (lower == 'z' || lower == 'x')
    {
        const int shifted = baseNote + (lower == 'z' ? -12 : 12);
        baseNote = juce::jlimit (kLowestBaseNote, kHighestBaseNote, shifted);
        return std::nullopt;
    }

    const int offset = offsetForKey (lower);

    if (offset < 0 || held.count (lower) > 0) // key repeat is ignored
        return std::nullopt;

    const int note = baseNote + offset;
    held[lower] = note;
    return NoteEvent { note, true };
}

std::optional<ComputerKeyboardInput::NoteEvent> ComputerKeyboardInput::keyUp (juce::juce_wchar key)
{
    const auto lower = juce::CharacterFunctions::toLowerCase (key);
    const auto it = held.find (lower);

    if (it == held.end())
        return std::nullopt;

    const NoteEvent event { it->second, false };
    held.erase (it);
    return event;
}

std::vector<juce::juce_wchar> ComputerKeyboardInput::getHeldKeys() const
{
    std::vector<juce::juce_wchar> keys;

    for (const auto& [key, note] : held)
        keys.push_back (key);

    return keys;
}

std::vector<ComputerKeyboardInput::NoteEvent> ComputerKeyboardInput::releaseAll()
{
    std::vector<NoteEvent> events;

    for (const auto& [key, note] : held)
        events.push_back ({ note, false });

    held.clear();
    return events;
}
} // namespace p5x::standalone

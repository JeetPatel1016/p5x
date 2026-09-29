// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#include "standalone/ComputerKeyboardInput.h"

#include <catch2/catch_test_macros.hpp>

using p5x::standalone::ComputerKeyboardInput;

TEST_CASE ("Computer keyboard: key down/up give matching note on/off", "[standalone][keyboard]")
{
    ComputerKeyboardInput keys;

    const auto on = keys.keyDown ('a');
    REQUIRE (on.has_value());
    REQUIRE (on->on);
    REQUIRE (on->note == 48); // default: A = C3

    const auto off = keys.keyUp ('a');
    REQUIRE (off.has_value());
    REQUIRE_FALSE (off->on);
    REQUIRE (off->note == 48);

    REQUIRE_FALSE (keys.keyUp ('a').has_value());
}

TEST_CASE ("Computer keyboard: the mapping from 10-debug-and-harness.md", "[standalone][keyboard]")
{
    ComputerKeyboardInput keys;
    const std::pair<char, int> expected[] { { 'a', 0 }, { 'w', 1 }, { 's', 2 }, { 'e', 3 }, { 'd', 4 },
                                            { 'f', 5 }, { 't', 6 }, { 'g', 7 }, { 'y', 8 }, { 'h', 9 },
                                            { 'u', 10 }, { 'j', 11 }, { 'k', 12 } };

    for (const auto& [key, offset] : expected)
    {
        const auto event = keys.keyDown ((juce::juce_wchar) key);
        REQUIRE (event.has_value());
        REQUIRE (event->note == 48 + offset);
    }

    REQUIRE_FALSE (keys.keyDown ('q').has_value());
    REQUIRE (ComputerKeyboardInput::offsetForKey ('A') == 0); // case-insensitive
}

TEST_CASE ("Computer keyboard: key repeat is ignored", "[standalone][keyboard]")
{
    ComputerKeyboardInput keys;
    REQUIRE (keys.keyDown ('d').has_value());
    REQUIRE_FALSE (keys.keyDown ('d').has_value());
    REQUIRE_FALSE (keys.keyDown ('D').has_value());
}

TEST_CASE ("Computer keyboard: octave shift clamps inside the 61-key range", "[standalone][keyboard]")
{
    ComputerKeyboardInput keys;

    for (int i = 0; i < 10; ++i)
        keys.keyDown ('z');

    REQUIRE (keys.getBaseNote() == 36);
    REQUIRE (keys.keyDown ('a')->note == 36);

    for (int i = 0; i < 10; ++i)
        keys.keyDown ('x');

    REQUIRE (keys.getBaseNote() == 84);
    REQUIRE (keys.keyDown ('k')->note == 96); // the top key stays ≤ 96
}

TEST_CASE ("Computer keyboard: a note keeps its pitch across an octave change", "[standalone][keyboard]")
{
    ComputerKeyboardInput keys;
    REQUIRE (keys.keyDown ('a')->note == 48);
    keys.keyDown ('x');

    const auto off = keys.keyUp ('a');
    REQUIRE (off->note == 48);
    REQUIRE (keys.keyDown ('a')->note == 60);
}

TEST_CASE ("Computer keyboard: releaseAll stops every held note", "[standalone][keyboard]")
{
    ComputerKeyboardInput keys;
    keys.keyDown ('a');
    keys.keyDown ('g');

    const auto offs = keys.releaseAll();
    REQUIRE (offs.size() == 2);
    REQUIRE (keys.getHeldKeys().empty());
}

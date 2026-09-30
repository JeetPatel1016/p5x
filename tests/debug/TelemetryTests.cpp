// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#include "debug/Telemetry.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <atomic>
#include <cmath>
#include <thread>

using p5x::debug::TripleBuffer;

TEST_CASE ("TripleBuffer: the reader gets the latest write, once", "[debug][telemetry]")
{
    TripleBuffer<int> buffer;
    int value = 0;

    REQUIRE_FALSE (buffer.read (value));

    buffer.write (1);
    buffer.write (2);
    buffer.write (3);

    REQUIRE (buffer.read (value));
    REQUIRE (value == 3);
    REQUIRE_FALSE (buffer.read (value));

    buffer.write (4);
    REQUIRE (buffer.read (value));
    REQUIRE (value == 4);
}

TEST_CASE ("TripleBuffer: concurrent writer and reader never see a torn snapshot", "[debug][telemetry]")
{
    struct Snapshot
    {
        int a = 0, b = 0, c = 0;
    };

    TripleBuffer<Snapshot> buffer;
    std::atomic<bool> done { false };

    std::thread writer ([&]
                        {
                            for (int i = 1; i <= 200000; ++i)
                                buffer.write ({ i, i * 2, i * 3 });

                            done.store (true);
                        });

    bool consistent = true;
    int lastSeen = 0;
    Snapshot s;

    while (! done.load())
    {
        if (buffer.read (s))
        {
            consistent = consistent && s.b == s.a * 2 && s.c == s.a * 3 && s.a >= lastSeen;
            lastSeen = s.a;
        }
    }

    writer.join();

    if (buffer.read (s))
        lastSeen = s.a;

    REQUIRE (consistent);
    REQUIRE (lastSeen == 200000);
}

TEST_CASE ("Scope trigger: first rising zero crossing, or 0 when there is none", "[debug][scope]")
{
    using p5x::debug::findScopeTrigger;

    const float rising[] = { 0.5f, 0.2f, -0.1f, -0.4f, 0.0f, 0.3f, -0.2f, 0.1f };
    REQUIRE (findScopeTrigger (rising, 7) == 4); // −0.4 → 0.0 counts (>= 0); 0.5 → 0.2 is falling
    REQUIRE (findScopeTrigger (rising, 3) == 0); // no crossing within the allowed start range

    const float silence[8] {};
    REQUIRE (findScopeTrigger (silence, 7) == 0);

    std::array<float, p5x::debug::kScopeSamples> sine {};

    for (size_t i = 0; i < sine.size(); ++i)
        sine[i] = (float) std::sin (0.1 * (double) i + 2.0); // starts falling from a positive value

    const int start = findScopeTrigger (sine.data(), p5x::debug::kScopeSamples / 2);
    REQUIRE (start > 0);
    REQUIRE (sine[(size_t) start - 1] < 0.0f);
    REQUIRE (sine[(size_t) start] >= 0.0f);
}

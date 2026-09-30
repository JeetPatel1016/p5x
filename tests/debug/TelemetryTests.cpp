// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#include "debug/Telemetry.h"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
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

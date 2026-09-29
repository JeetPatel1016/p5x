// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#include "debug/DebugLog.h"
#include "debug/LogFileSink.h"
#include "debug/MpscRing.h"
#include "support/AllocationCounter.h"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

using namespace p5x::debug;

TEST_CASE ("MpscRing: FIFO order, full and empty", "[debug][log]")
{
    MpscRing<int, 4> ring;

    for (int i = 0; i < 4; ++i)
        REQUIRE (ring.tryPush (i));

    REQUIRE_FALSE (ring.tryPush (99)); // full

    int value = -1;

    for (int i = 0; i < 4; ++i)
    {
        REQUIRE (ring.tryPop (value));
        REQUIRE (value == i);
    }

    REQUIRE_FALSE (ring.tryPop (value)); // empty
    REQUIRE (ring.tryPush (5));          // wraps around
    REQUIRE (ring.tryPop (value));
    REQUIRE (value == 5);
}

TEST_CASE ("MpscRing: many producers, one consumer, nothing lost or reordered per producer", "[debug][log]")
{
    struct Item
    {
        int producer = 0, sequence = 0;
    };

    constexpr int producers = 4, perProducer = 20000;
    MpscRing<Item, 1024> ring;
    std::atomic<int> drops { 0 };
    std::atomic<bool> done { false };

    std::vector<std::thread> threads;

    for (int p = 0; p < producers; ++p)
        threads.emplace_back ([&ring, &drops, p]
                              {
                                  for (int s = 0; s < perProducer; ++s)
                                      if (! ring.tryPush ({ p, s }))
                                          drops.fetch_add (1);
                              });

    std::vector<int> lastSeen (producers, -1);
    int received = 0;
    bool ordered = true;

    std::thread consumer ([&]
                          {
                              Item item;

                              for (;;)
                              {
                                  const bool finished = done.load(); // read before popping, so nothing is missed

                                  if (ring.tryPop (item))
                                  {
                                      ordered = ordered && item.sequence > lastSeen[(size_t) item.producer];
                                      lastSeen[(size_t) item.producer] = item.sequence;
                                      ++received;
                                  }
                                  else if (finished)
                                  {
                                      break;
                                  }
                              }
                          });

    for (auto& t : threads)
        t.join();

    done.store (true);
    consumer.join();

    REQUIRE (ordered);
    REQUIRE (received + drops.load() == producers * perProducer);
}

TEST_CASE ("Logger: entries carry level, source and instance; text is truncated", "[debug][log]")
{
    Logger logger;
    const std::string longText (300, 'x');
    logger.log (Level::Warn, Source::Engine, 7, "%s", longText.c_str());

    LogEntry entry;
    REQUIRE (logger.pop (entry));
    REQUIRE (entry.level == (uint8_t) Level::Warn);
    REQUIRE (entry.source == (uint8_t) Source::Engine);
    REQUIRE (entry.instanceId == 7);
    REQUIRE (std::strlen (entry.text) == sizeof (entry.text) - 1);
    REQUIRE (entry.timeMs > 0);
    REQUIRE_FALSE (logger.pop (entry));
}

TEST_CASE ("Logger: the macros reach the current logger; DEBUG obeys the build type", "[debug][log]")
{
    Logger logger;
    P5X_LOG (Info, Midi, 3, "hello %d", 42);
    P5X_LOG (Debug, Midi, 3, "debug %d", 1);

    LogEntry entry;
    REQUIRE (logger.pop (entry));
    REQUIRE (std::string (entry.text) == "hello 42");
    REQUIRE (logger.pop (entry) == (P5X_LOG_DEBUG_ENABLED == 1));
}

TEST_CASE ("Logger: entries from other threads and the message thread are merged by time", "[debug][log]")
{
    Logger logger;
    logger.log (Level::Info, Source::Ui, 1, "first (message thread)");

    std::thread other ([&logger] { logger.log (Level::Info, Source::Engine, 1, "second (audio thread)"); });
    other.join();

    LogEntry a, b;
    REQUIRE (logger.pop (a));
    REQUIRE (logger.pop (b));
    REQUIRE (a.timeMs <= b.timeMs);
}

TEST_CASE ("Logger: 10 000 entries/s for 10 s from the audio thread: no allocation, drops counted", "[debug][log][rt]")
{
    Logger logger;
    constexpr int total = 100000; // 10 000/s × 10 s, pushed as fast as possible
    std::atomic<bool> producerDone { false };
    std::atomic<int64_t> producerAllocations { -1 };

    std::thread audio ([&]
                       {
                           {
                               p5x::test::ScopedAllocationCounter counter;

                               for (int i = 0; i < total; ++i)
                                   P5X_LOG (Warn, Engine, 1, "Voice %d stolen: note %d -> %d", i % 10, 60, 61);

                               producerAllocations.store (counter.count());
                           }

                           producerDone.store (true);
                       });

    // The drainer runs concurrently, as the 30 Hz timer would.
    int popped = 0;
    LogEntry entry;

    while (! producerDone.load())
        while (logger.pop (entry))
            ++popped;

    audio.join();

    while (logger.pop (entry))
        ++popped;

    REQUIRE (producerAllocations.load() == 0);
    REQUIRE (popped + (int) logger.takeDroppedCount() == total);
}

TEST_CASE ("Logger: no current logger means entries are discarded safely", "[debug][log]")
{
    REQUIRE (Logger::get() == nullptr);
    P5X_LOG (Error, Log, 0, "nobody is listening");
    SUCCEED();
}

TEST_CASE ("RateLimiter: allows at most N per second", "[debug][log]")
{
    RateLimiter limiter;
    int allowed = 0;

    for (int i = 0; i < 100; ++i)
        allowed += limiter.allow (10) ? 1 : 0;

    REQUIRE (allowed == 10);
}

TEST_CASE ("Instance ids are unique and non-zero", "[debug][log]")
{
    const auto a = nextInstanceId(), b = nextInstanceId();
    REQUIRE (a != 0);
    REQUIRE (b != a);
}

TEST_CASE ("LogFileSink: line format and keeping the newest 5 files", "[debug][log]")
{
    const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("p5x-logsink-test");
    dir.deleteRecursively();
    dir.createDirectory();

    for (const char* day : { "2026-09-01", "2026-09-02", "2026-09-03", "2026-09-04", "2026-09-05", "2026-09-06" })
        dir.getChildFile (juce::String ("p5x-") + day + ".log").replaceWithText ("old");

    LogEntry entry;
    entry.timeMs = (uint64_t) juce::Time::currentTimeMillis();
    entry.level = (uint8_t) Level::Warn;
    entry.source = (uint8_t) Source::Engine;
    entry.instanceId = 2;
    std::strcpy (entry.text, "Voice 3 stolen");

    const auto line = LogFileSink::formatLine (entry);
    REQUIRE (line.contains ("WARN"));
    REQUIRE (line.contains ("#2"));
    REQUIRE (line.contains ("engine"));
    REQUIRE (line.endsWith ("Voice 3 stolen"));

    {
        LogFileSink sink (dir, 5);
        sink.write (entry);
        sink.flush();
        REQUIRE (sink.getCurrentFile().loadFileAsString().contains ("Voice 3 stolen"));
    }

    const auto files = dir.findChildFiles (juce::File::findFiles, false, "p5x-*.log");
    REQUIRE (files.size() == 5);
    REQUIRE_FALSE (dir.getChildFile ("p5x-2026-09-01.log").exists());
    REQUIRE_FALSE (dir.getChildFile ("p5x-2026-09-02.log").exists());

    dir.deleteRecursively();
}

// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#pragma once

#include "debug/DebugLog.h"
#include "debug/LogFileSink.h"

#include <juce_events/juce_events.h>

#include <vector>

namespace p5x::debug
{
// Owns the process-wide Logger and its single drainer (10-debug-and-harness.md § Logging).
// Held through juce::SharedResourcePointer: created with the first plugin instance, destroyed with
// the last. The 30 Hz drainer moves entries into a shared history of the last 5 000 and the file sink.
class LogService : private juce::Timer
{
public:
    static constexpr size_t kHistorySize = 5000;

    LogService();
    ~LogService() override;

    Logger& getLogger() noexcept { return logger; }
    juce::File getLogFile() const { return sink.getCurrentFile(); }

    // Total entries ever drained; entry n lives at history[n % kHistorySize] while n ≥ total − size.
    uint64_t getTotalCount() const noexcept { return total; }

    // Appends entries with sequence ≥ fromSequence (clamped to what's still kept) and returns the
    // next sequence to ask for.
    uint64_t copyEntriesSince (uint64_t fromSequence, std::vector<LogEntry>& out) const;

    // Drains immediately (normally the timer does this).
    void drainNow();

private:
    void timerCallback() override { drainNow(); }

    Logger logger;
    LogFileSink sink;
    std::vector<LogEntry> history;
    uint64_t total = 0;
};
} // namespace p5x::debug

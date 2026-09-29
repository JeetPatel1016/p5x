// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#include "debug/LogService.h"
#include "AppPaths.h"

namespace p5x::debug
{
LogService::LogService()
    : sink (paths::logsDir())
{
    history.resize (kHistorySize);
    startTimerHz (30);
}

LogService::~LogService()
{
    stopTimer();
    drainNow();
    sink.flush();
}

void LogService::drainNow()
{
    if (const auto dropped = logger.takeDroppedCount(); dropped > 0)
        logger.log (Level::Warn, Source::Log, 0, "%llu log entries dropped (FIFO full)", (unsigned long long) dropped);

    LogEntry entry;
    bool wroteAny = false;

    while (logger.pop (entry))
    {
        history[(size_t) (total % kHistorySize)] = entry;
        ++total;
        sink.write (entry);
        wroteAny = true;
    }

    if (wroteAny)
        sink.flush();
}

uint64_t LogService::copyEntriesSince (uint64_t fromSequence, std::vector<LogEntry>& out) const
{
    const uint64_t oldest = total > kHistorySize ? total - kHistorySize : 0;

    for (uint64_t n = std::max (fromSequence, oldest); n < total; ++n)
        out.push_back (history[(size_t) (n % kHistorySize)]);

    return total;
}
} // namespace p5x::debug

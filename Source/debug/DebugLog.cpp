// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#include "debug/DebugLog.h"

#include <chrono>
#include <cstdio>

namespace p5x::debug
{
namespace
{
std::atomic<Logger*> currentLogger { nullptr };
std::atomic<uint16_t> instanceCounter { 0 };

uint64_t wallClockMs() noexcept
{
    using namespace std::chrono;
    return (uint64_t) duration_cast<milliseconds> (system_clock::now().time_since_epoch()).count();
}

int64_t steadyMs() noexcept
{
    using namespace std::chrono;
    return (int64_t) duration_cast<milliseconds> (steady_clock::now().time_since_epoch()).count();
}
} // namespace

const char* levelName (Level level) noexcept
{
    switch (level)
    {
        case Level::Error: return "ERROR";
        case Level::Warn:  return "WARN";
        case Level::Info:  return "INFO";
        case Level::Debug: return "DEBUG";
        default:           return "?";
    }
}

const char* sourceName (Source source) noexcept
{
    switch (source)
    {
        case Source::Engine:     return "engine";
        case Source::Midi:       return "midi";
        case Source::Learn:      return "learn";
        case Source::State:      return "state";
        case Source::Ui:         return "ui";
        case Source::Standalone: return "standalone";
        case Source::Log:        return "log";
        default:                 return "?";
    }
}

//==================================================================================================
Logger::Logger() noexcept
{
    messageThread.store (std::this_thread::get_id());
    currentLogger.store (this);
}

Logger::~Logger()
{
    Logger* expected = this;
    currentLogger.compare_exchange_strong (expected, nullptr);
}

Logger* Logger::get() noexcept
{
    return currentLogger.load (std::memory_order_acquire);
}

void Logger::log (Level level, Source source, uint16_t instanceId, const char* format, ...) noexcept
{
    va_list args;
    va_start (args, format);
    logV (level, source, instanceId, format, args);
    va_end (args);
}

void Logger::logV (Level level, Source source, uint16_t instanceId, const char* format, va_list args) noexcept
{
    LogEntry entry;
    entry.timeMs = wallClockMs();
    entry.instanceId = instanceId;
    entry.level = (uint8_t) level;
    entry.source = (uint8_t) source;

    // Fixed-size formatting; longer text is truncated (vsnprintf always terminates).
    std::vsnprintf (entry.text, sizeof (entry.text), format, args);

    auto& ring = (std::this_thread::get_id() == messageThread.load()) ? messageThreadRing : otherThreads;

    if (! ring.tryPush (entry))
        dropped.fetch_add (1, std::memory_order_relaxed);
}

bool Logger::pop (LogEntry& entry) noexcept
{
    return popFromRings (entry);
}

bool Logger::popFromRings (LogEntry& entry) noexcept
{
    if (! hasPendingOther)
        hasPendingOther = otherThreads.tryPop (pendingOther);

    if (! hasPendingMessage)
        hasPendingMessage = messageThreadRing.tryPop (pendingMessage);

    if (! hasPendingOther && ! hasPendingMessage)
        return false;

    const bool takeOther = hasPendingOther && (! hasPendingMessage || pendingOther.timeMs <= pendingMessage.timeMs);

    if (takeOther)
    {
        entry = pendingOther;
        hasPendingOther = false;
    }
    else
    {
        entry = pendingMessage;
        hasPendingMessage = false;
    }

    return true;
}

//==================================================================================================
bool RateLimiter::allow (int perSecond) noexcept
{
    const int64_t now = steadyMs();
    int64_t start = windowStart.load (std::memory_order_relaxed);

    if (now - start >= 1000 && windowStart.compare_exchange_strong (start, now, std::memory_order_relaxed))
        count.store (0, std::memory_order_relaxed);

    return count.fetch_add (1, std::memory_order_relaxed) < perSecond;
}

void logFormatted (Level level, Source source, uint16_t instanceId, const char* format, ...) noexcept
{
    auto* logger = Logger::get();

    if (logger == nullptr)
        return;

    va_list args;
    va_start (args, format);
    logger->logV (level, source, instanceId, format, args);
    va_end (args);
}

uint16_t nextInstanceId() noexcept
{
    return (uint16_t) (instanceCounter.fetch_add (1) + 1);
}
} // namespace p5x::debug

// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#pragma once

#include "debug/MpscRing.h"

#include <atomic>
#include <cstdarg>
#include <cstdint>
#include <thread>

namespace p5x::debug
{
enum class Level : uint8_t
{
    Error,
    Warn,
    Info,
    Debug
};

// The subsystem that wrote an entry (10-debug-and-harness.md § Logging).
enum class Source : uint8_t
{
    Engine, // processor audio path: voices, steals, clipper
    Midi,
    Learn,
    State,
    Ui,
    Standalone,
    Log // the logger itself, e.g. drop counts
};

// Fixed-size POD so producing an entry never allocates.
struct LogEntry
{
    uint64_t timeMs = 0; // wall clock, ms since the Unix epoch
    uint16_t instanceId = 0; // 0 = global, not tied to a plugin instance
    uint8_t level = 0;
    uint8_t source = 0;
    char text[120] {};
};

const char* levelName (Level level) noexcept;
const char* sourceName (Source source) noexcept;

// Process-wide logger shared by every plugin instance. Callable from any thread:
//  - the message thread writes into its own ring,
//  - every other thread (each instance's audio thread, host worker threads) shares a multi-producer ring.
// One consumer (the message-thread drainer in LogService) pops from both.
class Logger
{
public:
    static constexpr size_t kFifoSize = 2048;

    Logger() noexcept;
    ~Logger();

    Logger (const Logger&) = delete;
    Logger& operator= (const Logger&) = delete;

    // The logger currently receiving P5X_LOG calls, or nullptr (entries are then discarded).
    static Logger* get() noexcept;

    void setMessageThread (std::thread::id id) noexcept { messageThread.store (id); }

    void log (Level level, Source source, uint16_t instanceId, const char* format, ...) noexcept;
    void logV (Level level, Source source, uint16_t instanceId, const char* format, va_list args) noexcept;

    // Consumer side (message thread). Pops the older of the two rings' heads.
    bool pop (LogEntry& entry) noexcept;
    uint64_t takeDroppedCount() noexcept { return dropped.exchange (0); }

private:
    bool popFromRings (LogEntry& entry) noexcept;

    MpscRing<LogEntry, kFifoSize> otherThreads;
    MpscRing<LogEntry, kFifoSize> messageThreadRing;
    std::atomic<std::thread::id> messageThread;
    std::atomic<uint64_t> dropped { 0 };

    LogEntry pendingOther, pendingMessage;
    bool hasPendingOther = false, hasPendingMessage = false;
};

// Lets a call site that can repeat per block log at most `perSecond` times per second.
class RateLimiter
{
public:
    bool allow (int perSecond) noexcept;

private:
    std::atomic<int64_t> windowStart { -1000000 };
    std::atomic<int> count { 0 };
};

// printf-style entry point used by the macros below.
void logFormatted (Level level, Source source, uint16_t instanceId, const char* format, ...) noexcept;

// Process-unique plugin instance numbers: 1, 2, ...
uint16_t nextInstanceId() noexcept;
} // namespace p5x::debug

// P5X_LOG (Level, Source, instanceId, format, args...)
//   e.g. P5X_LOG (Warn, Engine, instanceId, "Voice %d stolen", voice);
// DEBUG entries compile out in Release unless P5X_VERBOSE=1.
#define P5X_LOG(level, source, instanceId, ...) P5X_LOG_##level (source, instanceId, __VA_ARGS__)

#define P5X_LOG_IMPL(level, source, instanceId, ...) \
    ::p5x::debug::logFormatted (::p5x::debug::Level::level, ::p5x::debug::Source::source, \
                                static_cast<uint16_t> (instanceId), __VA_ARGS__)

#define P5X_LOG_Error(source, instanceId, ...) P5X_LOG_IMPL (Error, source, instanceId, __VA_ARGS__)
#define P5X_LOG_Warn(source, instanceId, ...) P5X_LOG_IMPL (Warn, source, instanceId, __VA_ARGS__)
#define P5X_LOG_Info(source, instanceId, ...) P5X_LOG_IMPL (Info, source, instanceId, __VA_ARGS__)

#if ! defined(NDEBUG) || P5X_VERBOSE
    #define P5X_LOG_DEBUG_ENABLED 1
    #define P5X_LOG_Debug(source, instanceId, ...) P5X_LOG_IMPL (Debug, source, instanceId, __VA_ARGS__)
#else
    #define P5X_LOG_DEBUG_ENABLED 0
    #define P5X_LOG_Debug(source, instanceId, ...) ((void) 0)
#endif

// P5X_LOG_RATE (Level, perSecond, Source, instanceId, format, args...)
#define P5X_LOG_RATE(level, perSecond, source, instanceId, ...) \
    do \
    { \
        static ::p5x::debug::RateLimiter p5xRateLimiter_; \
        if (p5xRateLimiter_.allow (perSecond)) \
            P5X_LOG (level, source, instanceId, __VA_ARGS__); \
    } while (false)

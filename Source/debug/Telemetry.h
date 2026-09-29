// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#pragma once

#include <array>
#include <atomic>
#include <cstdint>

namespace p5x::debug
{
// Single-producer, single-consumer triple buffer: the writer never waits, the reader always gets
// the latest complete snapshot (00-architecture.md § Cross-thread channels).
template <typename T>
class TripleBuffer
{
public:
    // Producer (audio thread).
    void write (const T& value) noexcept
    {
        buffers[(size_t) back] = value;
        const uint8_t previous = shared.exchange ((uint8_t) (back | kDirty), std::memory_order_acq_rel);
        back = previous & kIndexMask;
    }

    // Consumer (message thread). Returns false when nothing new was written since the last read.
    bool read (T& value) noexcept
    {
        if ((shared.load (std::memory_order_acquire) & kDirty) == 0)
            return false;

        const uint8_t previous = shared.exchange (front, std::memory_order_acq_rel);
        front = previous & kIndexMask;
        value = buffers[(size_t) front];
        return true;
    }

private:
    static constexpr uint8_t kDirty = 0x4;
    static constexpr uint8_t kIndexMask = 0x3;

    std::array<T, 3> buffers {};
    std::atomic<uint8_t> shared { 1 };
    uint8_t back = 0;  // producer-owned index
    uint8_t front = 2; // consumer-owned index
};

// Milestone 1 telemetry (10-debug-and-harness.md § Telemetry). Per-voice data and the scope arrive
// with the Voices table (milestone 2) and the scope (milestone 3).
struct TelemetrySnapshot
{
    float cpuPercent = 0.0f;
    double sampleRate = 0.0;
    int blockSize = 0;
    int oversampling = 1;
    uint32_t xruns = 0;
    int activeVoices = 0;
    int voiceCount = 5;
};
} // namespace p5x::debug

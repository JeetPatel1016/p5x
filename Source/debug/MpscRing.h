// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace p5x::debug
{
// Bounded lock-free queue: many producers, one consumer. Each cell carries a sequence number that
// says whose turn it is (the scheme described by D. Vyukov for bounded MPMC queues), so producers
// only contend on one atomic and never wait for each other. tryPush() never blocks or allocates;
// when full it fails and the caller counts the drop (10-debug-and-harness.md § Logging).
template <typename T, size_t Capacity>
class MpscRing
{
    static_assert (Capacity >= 2 && (Capacity & (Capacity - 1)) == 0, "Capacity must be a power of two");

public:
    MpscRing() noexcept
    {
        for (size_t i = 0; i < Capacity; ++i)
            cells[i].sequence.store (i, std::memory_order_relaxed);
    }

    // Any thread.
    bool tryPush (const T& value) noexcept
    {
        size_t pos = tail.load (std::memory_order_relaxed);

        for (;;)
        {
            Cell& cell = cells[pos & kMask];
            const size_t seq = cell.sequence.load (std::memory_order_acquire);
            const auto diff = static_cast<std::intptr_t> (seq) - static_cast<std::intptr_t> (pos);

            if (diff == 0)
            {
                if (tail.compare_exchange_weak (pos, pos + 1, std::memory_order_relaxed))
                {
                    cell.value = value;
                    cell.sequence.store (pos + 1, std::memory_order_release);
                    return true;
                }
            }
            else if (diff < 0)
            {
                return false; // full
            }
            else
            {
                pos = tail.load (std::memory_order_relaxed);
            }
        }
    }

    // The single consumer thread only.
    bool tryPop (T& out) noexcept
    {
        Cell& cell = cells[head & kMask];
        const size_t seq = cell.sequence.load (std::memory_order_acquire);

        if (seq != head + 1)
            return false; // empty, or the producer hasn't finished writing this cell yet

        out = cell.value;
        cell.sequence.store (head + Capacity, std::memory_order_release);
        ++head;
        return true;
    }

private:
    static constexpr size_t kMask = Capacity - 1;

    struct Cell
    {
        std::atomic<size_t> sequence { 0 };
        T value {};
    };

    std::array<Cell, Capacity> cells;
    std::atomic<size_t> tail { 0 };
    size_t head = 0;
};
} // namespace p5x::debug

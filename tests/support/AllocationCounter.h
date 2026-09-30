// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#pragma once

#include <cstdint>

namespace p5x::test
{
// Counts operator new calls made on the current thread while alive (11-testing.md § Real-time safety).
// The global operator new/delete replacements live in AllocationCounter.cpp.
class ScopedAllocationCounter
{
public:
    ScopedAllocationCounter();
    ~ScopedAllocationCounter();

    int64_t count() const;
};
} // namespace p5x::test

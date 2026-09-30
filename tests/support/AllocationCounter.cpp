// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#include "support/AllocationCounter.h"

#include <cstdlib>
#include <malloc.h>
#include <new>

namespace
{
thread_local bool counting = false;
thread_local int64_t allocations = 0;

void* allocate (std::size_t size)
{
    if (counting)
        ++allocations;

    if (void* p = std::malloc (size == 0 ? 1 : size))
        return p;

    throw std::bad_alloc();
}

void* allocateAligned (std::size_t size, std::align_val_t alignment)
{
    if (counting)
        ++allocations;

    if (void* p = _aligned_malloc (size == 0 ? 1 : size, static_cast<std::size_t> (alignment)))
        return p;

    throw std::bad_alloc();
}
} // namespace

namespace p5x::test
{
ScopedAllocationCounter::ScopedAllocationCounter()
{
    allocations = 0;
    counting = true;
}

ScopedAllocationCounter::~ScopedAllocationCounter()
{
    counting = false;
}

int64_t ScopedAllocationCounter::count() const
{
    return allocations;
}
} // namespace p5x::test

// Global replacements: every allocation in the test binary goes through here.
void* operator new (std::size_t size) { return allocate (size); }
void* operator new[] (std::size_t size) { return allocate (size); }
void* operator new (std::size_t size, const std::nothrow_t&) noexcept
{
    try { return allocate (size); } catch (...) { return nullptr; }
}
void* operator new[] (std::size_t size, const std::nothrow_t&) noexcept
{
    try { return allocate (size); } catch (...) { return nullptr; }
}
void* operator new (std::size_t size, std::align_val_t alignment) { return allocateAligned (size, alignment); }
void* operator new[] (std::size_t size, std::align_val_t alignment) { return allocateAligned (size, alignment); }

void operator delete (void* p) noexcept { std::free (p); }
void operator delete[] (void* p) noexcept { std::free (p); }
void operator delete (void* p, std::size_t) noexcept { std::free (p); }
void operator delete[] (void* p, std::size_t) noexcept { std::free (p); }
void operator delete (void* p, const std::nothrow_t&) noexcept { std::free (p); }
void operator delete[] (void* p, const std::nothrow_t&) noexcept { std::free (p); }
void operator delete (void* p, std::align_val_t) noexcept { _aligned_free (p); }
void operator delete[] (void* p, std::align_val_t) noexcept { _aligned_free (p); }
void operator delete (void* p, std::size_t, std::align_val_t) noexcept { _aligned_free (p); }
void operator delete[] (void* p, std::size_t, std::align_val_t) noexcept { _aligned_free (p); }

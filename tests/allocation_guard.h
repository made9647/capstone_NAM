#pragma once
#include <cstddef>
#include <cstdlib>
#include <new>

// Include in one translation unit only. Track C++ allocations used by vectors
// and model construction; the firmware processing path uses no C allocators.
static std::size_t allocation_count = 0;
void *operator new(std::size_t size) {
    ++allocation_count;
    if (void *p = std::malloc(size ? size : 1))
        return p;
    throw std::bad_alloc();
}
void *operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void *p) noexcept { std::free(p); }
void operator delete[](void *p) noexcept { std::free(p); }
void operator delete(void *p, std::size_t) noexcept { std::free(p); }
void operator delete[](void *p, std::size_t) noexcept { std::free(p); }

#include "mealkit/core/AllocCount.h"

#include <cstdlib>
#include <new>

namespace mealkit::alloc {
namespace {
thread_local u64 t_count = 0;
} // namespace

#if MEALKIT_ALLOC_COUNT
bool counting() { return true; }
#else
bool counting() { return false; }
#endif
u64 threadCount() { return t_count; }

} // namespace mealkit::alloc

#if MEALKIT_ALLOC_COUNT
// Replaceable global allocation functions (same malloc/_aligned_malloc heap as the CRT's own, so frees match)
namespace {

void* allocate(size_t size) {
    ++mealkit::alloc::t_count;
    return std::malloc(size ? size : 1);
}

void* allocateAligned(size_t size, std::align_val_t align) {
    ++mealkit::alloc::t_count;
#if defined(_WIN32)
    return _aligned_malloc(size ? size : 1, static_cast<size_t>(align));
#else
    const size_t a = static_cast<size_t>(align);
    return std::aligned_alloc(a, ((size ? size : 1) + a - 1) / a * a);
#endif
}

void freeAligned(void* p) {
#if defined(_WIN32)
    _aligned_free(p);
#else
    std::free(p);
#endif
}

} // namespace

void* operator new(size_t size) {
    if (void* p = allocate(size)) return p;
    throw std::bad_alloc();
}
void* operator new[](size_t size) { return operator new(size); }
void* operator new(size_t size, const std::nothrow_t&) noexcept { return allocate(size); }
void* operator new[](size_t size, const std::nothrow_t&) noexcept { return allocate(size); }
void* operator new(size_t size, std::align_val_t align) {
    if (void* p = allocateAligned(size, align)) return p;
    throw std::bad_alloc();
}
void* operator new[](size_t size, std::align_val_t align) { return operator new(size, align); }
void* operator new(size_t size, std::align_val_t align, const std::nothrow_t&) noexcept { return allocateAligned(size, align); }
void* operator new[](size_t size, std::align_val_t align, const std::nothrow_t&) noexcept { return allocateAligned(size, align); }

void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, size_t) noexcept { std::free(p); }
void operator delete[](void* p, size_t) noexcept { std::free(p); }
void operator delete(void* p, const std::nothrow_t&) noexcept { std::free(p); }
void operator delete[](void* p, const std::nothrow_t&) noexcept { std::free(p); }
void operator delete(void* p, std::align_val_t) noexcept { freeAligned(p); }
void operator delete[](void* p, std::align_val_t) noexcept { freeAligned(p); }
void operator delete(void* p, size_t, std::align_val_t) noexcept { freeAligned(p); }
void operator delete[](void* p, size_t, std::align_val_t) noexcept { freeAligned(p); }
void operator delete(void* p, std::align_val_t, const std::nothrow_t&) noexcept { freeAligned(p); }
void operator delete[](void* p, std::align_val_t, const std::nothrow_t&) noexcept { freeAligned(p); }
#endif

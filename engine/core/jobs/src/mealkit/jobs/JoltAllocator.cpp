#include "mealkit/jobs/JoltAllocator.h"

#include "mealkit/core/Memory.h"

#include <Jolt/Jolt.h>

#include <Jolt/Core/Memory.h>

#include <algorithm>
#include <cstring>
#include <mutex>

namespace mealkit::jobs {
namespace {

// Jolt's Free has no size, so a 16-byte header before each block keeps the size, the offset back to the start and the alignment
struct Header {
    u64 size;
    u32 offset;
    u32 align;
};
static_assert(sizeof(Header) == 16);

mem::Tag joltTag() {
    static const mem::Tag t = mem::tag("jolt");
    return t;
}

void* alignedAllocate(size_t size, size_t align) {
    align = std::max<size_t>(align, 16);
    const size_t offset = std::max<size_t>(align, sizeof(Header)); // the header fits before the aligned block
    auto* base = static_cast<unsigned char*>(mem::allocate(joltTag(), offset + size, align));
    unsigned char* block = base + offset;
    *reinterpret_cast<Header*>(block - sizeof(Header)) = {size, static_cast<u32>(offset), static_cast<u32>(align)};
    return block;
}

void alignedFree(void* p) {
    if (!p) return;
    auto* block = static_cast<unsigned char*>(p);
    const Header h = *reinterpret_cast<const Header*>(block - sizeof(Header));
    mem::deallocate(joltTag(), block - h.offset, h.offset + h.size, h.align);
}

void* allocate(size_t size) { return alignedAllocate(size, 16); }

void* reallocate(void* p, size_t oldSize, size_t newSize) {
    void* n = allocate(newSize);
    if (p) {
        std::memcpy(n, p, std::min(oldSize, newSize));
        alignedFree(p);
    }
    return n;
}

} // namespace

void installJoltAllocator() {
    static std::once_flag once;
    std::call_once(once, [] {
        // Free and AlignedFree are the same function: every block has the header, so a mismatched pair can't corrupt the heap
        JPH::Allocate = allocate;
        JPH::Reallocate = reallocate;
        JPH::Free = alignedFree;
        JPH::AlignedAllocate = alignedAllocate;
        JPH::AlignedFree = alignedFree;
    });
}

} // namespace mealkit::jobs

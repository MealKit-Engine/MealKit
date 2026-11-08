#pragma once

#include "mealkit/core/Types.h"

#include <cstddef>
#include <memory_resource>
#include <new>
#include <string_view>
#include <type_traits>

// Kernel memory (docs/ENGINE_MODULES.md "Memory management"): where memory comes from and who used it.
//
// - Tags name the user. One tag per engine module ("render", "physics", "sim", ...), registered on first use by name.
//   Every allocation through heap(tag), Allocator<T, Name>, an Arena or a FixedPool is counted under its tag.
//   stats(tag) is what FrameStats, the benchmark and the editor [Debug] > [Memory] page show.
// - One allocator interface: std::pmr::memory_resource. heap(tag) is the global heap counted under the tag;
//   std::pmr containers take it (std::pmr::vector<int> v{&mem::heap(tag)}).
// - Arena: linear, freed all at once with reset(). Pages come from the heap under its tag. Not thread safe.
// - FixedPool: same-size blocks with a free list (many small objects of one kind). Not thread safe.
// - frameArena() is reset at the end of each frame (game exe main loop, editor), matchArena() when a match stops.
//   Both are main thread only. Nothing may keep pointers into them past those points.
//
// sim/ code doesn't allocate inside a tick (docs/CODING_MODEL.md); Pool storage is counted under "sim" (sim/Pool.h).
// Counting is relaxed atomics, cheap enough to stay on in shipping builds (no per-allocation lock).
namespace mealkit::mem {

inline constexpr u32 kMaxTags = 64;
inline constexpr u32 kMaxTagName = 32;

struct Tag {
    u16 index = 0; // 0 = "other" (allocations nobody claimed)
    friend bool operator==(Tag a, Tag b) { return a.index == b.index; }
};

// Registers the name on first use and returns the same tag after that (thread safe). Past kMaxTags names, returns "other".
Tag tag(std::string_view name);
u32 tagCount(); // registered tags including "other"; valid indexes are 0..tagCount()-1
const char* tagName(Tag tag);

struct TagStats {
    u64 bytes = 0;       // currently allocated (arenas: reserved pages)
    u64 peak = 0;        // highest bytes since start
    u64 allocations = 0; // allocation calls since start
};
TagStats stats(Tag tag);

// Counted heap allocation (operator new with alignment). bytes and align must match on free.
void* allocate(Tag tag, size_t bytes, size_t align = alignof(std::max_align_t));
void deallocate(Tag tag, void* p, size_t bytes, size_t align = alignof(std::max_align_t));
// Adds to or removes from a tag's count without allocating (memory a library got some other way)
void count(Tag tag, i64 bytes);

// The global heap counted under the tag. One object per tag, lives until exit.
std::pmr::memory_resource& heap(Tag tag);

// Allocator for std containers that must stay plain std types (copied, hashed, saved byte-wise). The tag is a type,
// so the allocator is stateless and containers copy as usual:
//   struct SimTag { static constexpr const char* kName = "sim"; };
//   std::vector<T, mem::Allocator<T, SimTag>> items;
template <class T, class Name>
struct Allocator {
    using value_type = T;
    using is_always_equal = std::true_type;

    Allocator() noexcept = default;
    template <class U>
    Allocator(const Allocator<U, Name>&) noexcept {}

    T* allocate(size_t n) { return static_cast<T*>(mem::allocate(id(), n * sizeof(T), alignof(T))); }
    void deallocate(T* p, size_t n) noexcept { mem::deallocate(id(), p, n * sizeof(T), alignof(T)); }

    template <class U>
    struct rebind {
        using other = Allocator<U, Name>;
    };
    template <class U>
    friend bool operator==(const Allocator&, const Allocator<U, Name>&) noexcept {
        return true;
    }

    static Tag id() {
        static const Tag t = tag(Name::kName);
        return t;
    }
};

// Linear allocator: allocations bump a pointer, reset() frees everything at once. Pages are kept for reuse
// (only the first page stays after trim()). Allocations bigger than a page get a page of their own.
class Arena final : public std::pmr::memory_resource {
public:
    explicit Arena(Tag tag, size_t pageBytes = 64 * 1024);
    ~Arena() override;
    Arena(const Arena&) = delete;
    Arena& operator=(const Arena&) = delete;

    void reset();         // everything allocated so far is gone; pages are kept
    void trim();          // reset() and give back every page but the first
    size_t used() const;  // bytes handed out since the last reset (including alignment padding)
    size_t reserved() const; // bytes of pages held

private:
    struct Page;
    void* do_allocate(size_t bytes, size_t align) override;
    void do_deallocate(void*, size_t, size_t) override {} // freed by reset()
    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override { return this == &other; }
    Page* newPage(size_t minBytes);

    Tag m_tag;
    size_t m_pageBytes;
    Page* m_first = nullptr;   // all pages, in order
    Page* m_current = nullptr; // page being filled
    size_t m_used = 0;
    size_t m_reserved = 0;
};

// Same-size blocks with a free list. Pages of blocksPerPage blocks come from the heap under the tag and are freed in the destructor.
class FixedPool {
public:
    FixedPool(Tag tag, size_t blockBytes, size_t blockAlign = alignof(std::max_align_t), u32 blocksPerPage = 64);
    ~FixedPool();
    FixedPool(const FixedPool&) = delete;
    FixedPool& operator=(const FixedPool&) = delete;

    void* allocate();
    void free(void* p);
    u32 count() const { return m_count; } // blocks in use
    u32 capacity() const { return m_capacity; }

    template <class T, class... Args>
    T* create(Args&&... args) {
        return new (allocate()) T(static_cast<Args&&>(args)...);
    }
    template <class T>
    void destroy(T* p) {
        if (!p) return;
        p->~T();
        free(p);
    }

private:
    struct Page;
    void addPage();

    Tag m_tag;
    size_t m_blockBytes;
    size_t m_blockAlign;
    u32 m_blocksPerPage;
    Page* m_pages = nullptr;
    void* m_free = nullptr;
    u32 m_count = 0;
    u32 m_capacity = 0;
};

Arena& frameArena(); // tag "frame". Reset by endFrame()
Arena& matchArena(); // tag "match". Reset by endMatch()
void endFrame();     // end of each frame: frame arena reset
void endMatch();     // after GameInstance::stop: match arena trimmed

} // namespace mealkit::mem

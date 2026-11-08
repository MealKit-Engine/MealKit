#include "mealkit/core/Memory.h"

#include "mealkit/core/Log.h"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <mutex>

namespace mealkit::mem {
namespace {

struct Counter {
    std::atomic<u64> bytes{0};
    std::atomic<u64> peak{0};
    std::atomic<u64> allocations{0};
};

struct Registry {
    Counter counters[kMaxTags];
    char names[kMaxTags][kMaxTagName] = {};
    std::atomic<u32> count{1};
    std::mutex lock; // registration only
    Registry() { std::memcpy(names[0], "other", 6); }
};

Registry& registry() {
    static Registry r; // constructed on first use, so tags registered from static initializers work
    return r;
}

Counter& counter(Tag t) {
    Registry& r = registry();
    return r.counters[t.index < r.count.load(std::memory_order_acquire) ? t.index : 0];
}

void add(Tag t, i64 bytes, bool isAllocation) {
    Counter& c = counter(t);
    if (isAllocation) c.allocations.fetch_add(1, std::memory_order_relaxed);
    const u64 now = c.bytes.fetch_add(static_cast<u64>(bytes), std::memory_order_relaxed) + static_cast<u64>(bytes);
    if (bytes <= 0) return;
    u64 peak = c.peak.load(std::memory_order_relaxed);
    while (now > peak && !c.peak.compare_exchange_weak(peak, now, std::memory_order_relaxed)) {}
}

class HeapResource final : public std::pmr::memory_resource {
public:
    Tag tag;

private:
    void* do_allocate(size_t bytes, size_t align) override { return mem::allocate(tag, bytes, align); }
    void do_deallocate(void* p, size_t bytes, size_t align) override { mem::deallocate(tag, p, bytes, align); }
    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
        return this == &other; // one object per tag
    }
};

size_t alignUp(size_t v, size_t a) { return (v + a - 1) & ~(a - 1); }

} // namespace

Tag tag(std::string_view name) {
    Registry& r = registry();
    if (name.empty()) return {};
    const u32 n = r.count.load(std::memory_order_acquire);
    for (u32 i = 0; i < n; ++i)
        if (name == r.names[i]) return {static_cast<u16>(i)};
    std::lock_guard<std::mutex> hold(r.lock);
    const u32 m = r.count.load(std::memory_order_relaxed);
    for (u32 i = n; i < m; ++i)
        if (name == r.names[i]) return {static_cast<u16>(i)};
    if (m >= kMaxTags) {
        MEALKIT_WARN("memory: more than %u tags, '%.*s' counts as other", kMaxTags, static_cast<int>(name.size()), name.data());
        return {};
    }
    const size_t len = std::min<size_t>(name.size(), kMaxTagName - 1);
    std::memcpy(r.names[m], name.data(), len);
    r.names[m][len] = '\0';
    r.count.store(m + 1, std::memory_order_release);
    return {static_cast<u16>(m)};
}

u32 tagCount() { return registry().count.load(std::memory_order_acquire); }

const char* tagName(Tag t) {
    Registry& r = registry();
    return t.index < tagCount() ? r.names[t.index] : r.names[0];
}

TagStats stats(Tag t) {
    const Counter& c = counter(t);
    return {c.bytes.load(std::memory_order_relaxed), c.peak.load(std::memory_order_relaxed), c.allocations.load(std::memory_order_relaxed)};
}

void* allocate(Tag t, size_t bytes, size_t align) {
    void* p = ::operator new(bytes ? bytes : 1, std::align_val_t(align));
    add(t, static_cast<i64>(bytes), true);
    return p;
}

void deallocate(Tag t, void* p, size_t bytes, size_t align) {
    if (!p) return;
    ::operator delete(p, std::align_val_t(align));
    add(t, -static_cast<i64>(bytes), false);
}

void count(Tag t, i64 bytes) { add(t, bytes, bytes > 0); }

std::pmr::memory_resource& heap(Tag t) {
    static HeapResource resources[kMaxTags] = {};
    HeapResource& h = resources[t.index < kMaxTags ? t.index : 0];
    h.tag = t; // same value on every call, so a race writes the same thing
    return h;
}

// ---- Arena

struct Arena::Page {
    Page* next;
    size_t size; // usable bytes after the header
    size_t used;
    unsigned char* data() { return reinterpret_cast<unsigned char*>(this + 1); }
};

Arena::Arena(Tag t, size_t pageBytes) : m_tag(t), m_pageBytes(std::max<size_t>(pageBytes, 256)) {}

Arena::~Arena() {
    for (Page* p = m_first; p;) {
        Page* next = p->next;
        mem::deallocate(m_tag, p, sizeof(Page) + p->size, alignof(std::max_align_t));
        p = next;
    }
}

Arena::Page* Arena::newPage(size_t minBytes) {
    const size_t size = std::max(m_pageBytes, minBytes);
    auto* page = static_cast<Page*>(mem::allocate(m_tag, sizeof(Page) + size, alignof(std::max_align_t)));
    page->next = nullptr;
    page->size = size;
    page->used = 0;
    m_reserved += size;
    return page;
}

void* Arena::do_allocate(size_t bytes, size_t align) {
    if (align == 0) align = 1;
    // Fill the current page, then later kept pages, then a new one
    for (Page* p = m_current; p; p = p->next) {
        const uintptr_t base = reinterpret_cast<uintptr_t>(p->data());
        const size_t start = alignUp(base + p->used, align) - base;
        if (start + bytes <= p->size) {
            m_used += start + bytes - p->used;
            p->used = start + bytes;
            m_current = p;
            return p->data() + start;
        }
    }
    Page* page = newPage(bytes + align);
    if (!m_first) {
        m_first = page;
    } else {
        Page* last = m_current ? m_current : m_first;
        while (last->next) last = last->next;
        last->next = page;
    }
    m_current = page;
    const uintptr_t base = reinterpret_cast<uintptr_t>(page->data());
    const size_t start = alignUp(base, align) - base;
    page->used = start + bytes;
    m_used += page->used;
    return page->data() + start;
}

void Arena::reset() {
    for (Page* p = m_first; p; p = p->next) p->used = 0;
    m_current = m_first;
    m_used = 0;
}

void Arena::trim() {
    reset();
    if (!m_first) return;
    for (Page* p = m_first->next; p;) {
        Page* next = p->next;
        m_reserved -= p->size;
        mem::deallocate(m_tag, p, sizeof(Page) + p->size, alignof(std::max_align_t));
        p = next;
    }
    m_first->next = nullptr;
}

size_t Arena::used() const { return m_used; }
size_t Arena::reserved() const { return m_reserved; }

// ---- FixedPool

struct FixedPool::Page {
    Page* next;
};

FixedPool::FixedPool(Tag t, size_t blockBytes, size_t blockAlign, u32 blocksPerPage)
    : m_tag(t), m_blockAlign(std::max(blockAlign, alignof(void*))), m_blocksPerPage(std::max<u32>(blocksPerPage, 1)) {
    m_blockBytes = alignUp(std::max(blockBytes, sizeof(void*)), m_blockAlign);
}

FixedPool::~FixedPool() {
    MEALKIT_ASSERT(m_count == 0); // blocks still in use when the pool goes away
    const size_t header = alignUp(sizeof(Page), m_blockAlign);
    for (Page* p = m_pages; p;) {
        Page* next = p->next;
        mem::deallocate(m_tag, p, header + m_blockBytes * m_blocksPerPage, m_blockAlign);
        p = next;
    }
}

void FixedPool::addPage() {
    const size_t header = alignUp(sizeof(Page), m_blockAlign);
    auto* raw = static_cast<unsigned char*>(mem::allocate(m_tag, header + m_blockBytes * m_blocksPerPage, std::max(m_blockAlign, alignof(Page))));
    auto* page = reinterpret_cast<Page*>(raw);
    page->next = m_pages;
    m_pages = page;
    // Thread the new blocks onto the free list so the first block is handed out first
    for (u32 i = m_blocksPerPage; i > 0; --i) {
        void* block = raw + header + m_blockBytes * (i - 1);
        *static_cast<void**>(block) = m_free;
        m_free = block;
    }
    m_capacity += m_blocksPerPage;
}

void* FixedPool::allocate() {
    if (!m_free) addPage();
    void* block = m_free;
    m_free = *static_cast<void**>(block);
    ++m_count;
    return block;
}

void FixedPool::free(void* p) {
    if (!p) return;
    *static_cast<void**>(p) = m_free;
    m_free = p;
    --m_count;
}

// ---- Frame and match arenas

Arena& frameArena() {
    static Arena a{tag("frame"), 256 * 1024};
    return a;
}

Arena& matchArena() {
    static Arena a{tag("match"), 1024 * 1024};
    return a;
}

void endFrame() { frameArena().reset(); }
void endMatch() { matchArena().trim(); }

} // namespace mealkit::mem

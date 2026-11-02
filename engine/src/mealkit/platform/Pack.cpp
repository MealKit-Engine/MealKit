// Pack files (platform/Pack.h): writing for the cook step, mounting and lookup for platform/Paths.cpp and the OS file functions.
#include "mealkit/platform/Pack.h"

#include "mealkit/core/Compress.h"
#include "mealkit/core/Hash.h"
#include "mealkit/platform/Paths.h"

#include <algorithm>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <set>

namespace mealkit {
namespace {

constexpr char kMagic[8] = {'M', 'K', 'P', 'A', 'K', 0, 0, 0};
constexpr u32 kVersion = 1;
constexpr u32 kCompressed = 1;

struct Header {
    char magic[8];
    u32 version;
    u32 count;
    u64 tableOffset;
    u64 contentHash;
};
static_assert(sizeof(Header) == 32);

struct Entry {
    u64 offset = 0;
    u32 stored = 0, size = 0, flags = 0;
};

struct Mounted {
    std::string file;
    FILE* f = nullptr;
    u64 hash = 0;
    unsigned long long time = 0;
    std::map<std::string, Entry> entries; // relative path → entry
    std::set<std::string> folders;        // relative folders ("assets", "assets/ui")
};

struct State {
    std::mutex mutex; // reads can come from the audio thread (clips) and job workers
    std::string root;
    std::vector<std::unique_ptr<Mounted>> packs;
};
State& state() {
    static State s;
    return s;
}

std::string normalize(std::string p) {
    std::replace(p.begin(), p.end(), '\\', '/');
    while (p.size() > 1 && p.back() == '/') p.pop_back();
    return p;
}

// Full path → path relative to the root, or false if outside it
bool relative(const std::string& path, std::string& out) {
    const State& s = state();
    if (s.packs.empty()) return false;
    const std::string p = normalize(path);
    if (p.size() <= s.root.size() || p.compare(0, s.root.size(), s.root) != 0 || p[s.root.size()] != '/') return false;
    out = p.substr(s.root.size() + 1);
    return true;
}

const Entry* find(const std::string& path, const Mounted** owner) {
    std::string rel;
    if (!relative(path, rel)) return nullptr;
    const State& s = state();
    for (size_t i = s.packs.size(); i-- > 0;) {
        auto it = s.packs[i]->entries.find(rel);
        if (it != s.packs[i]->entries.end()) {
            if (owner) *owner = s.packs[i].get();
            return &it->second;
        }
    }
    return nullptr;
}

u64 hashItems(const std::vector<std::pair<const std::string*, const std::string*>>& items) {
    Hasher h;
    for (const auto& [path, bytes] : items) {
        h.bytes(path->data(), path->size());
        h.add(static_cast<u64>(bytes->size()));
        h.bytes(bytes->data(), bytes->size());
    }
    return h.value;
}

} // namespace

void PackWriter::add(const std::string& path, std::string_view bytes, bool compress) {
    const std::string p = normalize(path);
    for (Item& i : m_items)
        if (i.path == p) {
            i.bytes.assign(bytes);
            i.compress = compress;
            return;
        }
    m_items.push_back({p, std::string(bytes), compress});
}

u64 PackWriter::bytes() const {
    u64 n = 0;
    for (const Item& i : m_items) n += i.bytes.size();
    return n;
}

u64 PackWriter::contentHash() const {
    std::vector<const Item*> sorted;
    for (const Item& i : m_items) sorted.push_back(&i);
    std::sort(sorted.begin(), sorted.end(), [](const Item* a, const Item* b) { return a->path < b->path; });
    std::vector<std::pair<const std::string*, const std::string*>> items;
    for (const Item* i : sorted) items.emplace_back(&i->path, &i->bytes);
    return hashItems(items);
}

bool PackWriter::write(const std::string& file, std::string& error) const {
    std::vector<const Item*> sorted;
    for (const Item& i : m_items) sorted.push_back(&i);
    std::sort(sorted.begin(), sorted.end(), [](const Item* a, const Item* b) { return a->path < b->path; });

    std::string out(sizeof(Header), '\0');
    std::string table;
    const auto put = [&table](const void* p, size_t n) { table.append(static_cast<const char*>(p), n); };
    for (const Item* i : sorted) {
        if (i->bytes.size() > 0xFFFFFFFFu) return error = i->path + " is over 4 GB", false;
        Entry e;
        e.offset = out.size();
        e.size = static_cast<u32>(i->bytes.size());
        std::vector<u8> packed;
        if (i->compress && !i->bytes.empty())
            packed = compressBytes(reinterpret_cast<const u8*>(i->bytes.data()), e.size);
        if (!packed.empty() && packed.size() < i->bytes.size()) { // stored as is when compression doesn't help
            e.flags = kCompressed;
            e.stored = static_cast<u32>(packed.size());
            out.append(reinterpret_cast<const char*>(packed.data()), packed.size());
        } else {
            e.stored = e.size;
            out += i->bytes;
        }
        const u32 len = static_cast<u32>(i->path.size());
        put(&len, 4);
        put(i->path.data(), len);
        put(&e.offset, 8);
        put(&e.stored, 4);
        put(&e.size, 4);
        put(&e.flags, 4);
    }
    Header hd{};
    std::memcpy(hd.magic, kMagic, sizeof(kMagic));
    hd.version = kVersion;
    hd.count = static_cast<u32>(sorted.size());
    hd.tableOffset = out.size();
    hd.contentHash = contentHash();
    std::memcpy(out.data(), &hd, sizeof(hd));
    out += table;
    const std::string folder = parentDirectory(file);
    if (!folder.empty()) createDirectories(folder);
    if (!writeTextFile(file, out)) return error = "cannot write " + file, false;
    return true;
}

bool mountPack(const std::string& file, std::string& error) {
    auto m = std::make_unique<Mounted>();
    m->file = file;
    m->time = fileWriteTime(file);
    m->f = openFile(file, "rb"); // before taking the lock: openFile asks the pack lookup too
    if (!m->f) return error = "cannot open " + file, false;
    const auto fail = [&](const char* why) {
        fclose(m->f);
        error = file + ": " + why;
        return false;
    };
    Header hd{};
    if (fread(&hd, sizeof(hd), 1, m->f) != 1 || std::memcmp(hd.magic, kMagic, sizeof(kMagic)) != 0) return fail("not a MealKit pack");
    if (hd.version != kVersion) return fail("pack of another engine version (cook it again)");
    fseek(m->f, 0, SEEK_END);
    const long end = ftell(m->f);
    if (end < 0 || hd.tableOffset > static_cast<u64>(end)) return fail("damaged pack");
    std::string table(static_cast<size_t>(static_cast<u64>(end) - hd.tableOffset), '\0');
    fseek(m->f, static_cast<long>(hd.tableOffset), SEEK_SET);
    if (!table.empty() && fread(table.data(), 1, table.size(), m->f) != table.size()) return fail("damaged pack");
    size_t p = 0;
    const auto get = [&](void* dst, size_t n) {
        if (p + n > table.size()) return false;
        std::memcpy(dst, table.data() + p, n);
        p += n;
        return true;
    };
    for (u32 i = 0; i < hd.count; ++i) {
        u32 len = 0;
        if (!get(&len, 4) || p + len > table.size()) return fail("damaged pack table");
        std::string path = table.substr(p, len);
        p += len;
        Entry e;
        if (!get(&e.offset, 8) || !get(&e.stored, 4) || !get(&e.size, 4) || !get(&e.flags, 4) ||
            e.offset + e.stored > hd.tableOffset)
            return fail("damaged pack table");
        for (size_t slash = path.find('/'); slash != std::string::npos; slash = path.find('/', slash + 1))
            m->folders.insert(path.substr(0, slash));
        m->entries.emplace(std::move(path), e);
    }
    m->hash = hd.contentHash;

    State& s = state();
    std::lock_guard lock(s.mutex);
    if (s.packs.empty()) s.root = normalize(parentDirectory(file));
    s.packs.push_back(std::move(m));
    return true;
}

void unmountPacks() {
    State& s = state();
    std::lock_guard lock(s.mutex);
    for (auto& m : s.packs) fclose(m->f);
    s.packs.clear();
    s.root.clear();
}

bool packsMounted() {
    State& s = state();
    std::lock_guard lock(s.mutex);
    return !s.packs.empty();
}

const std::string& packRoot() { return state().root; }

u64 mountedPackHash() {
    State& s = state();
    std::lock_guard lock(s.mutex);
    Hasher h;
    for (const auto& m : s.packs) h.add(m->hash);
    return s.packs.empty() ? 0 : s.packs.size() == 1 ? s.packs[0]->hash : h.value;
}

namespace pack {

bool isFile(const std::string& path) {
    State& s = state();
    std::lock_guard lock(s.mutex);
    return find(path, nullptr) != nullptr;
}

bool isDirectory(const std::string& path) {
    State& s = state();
    std::lock_guard lock(s.mutex);
    if (s.packs.empty()) return false;
    if (normalize(path) == s.root) return true;
    std::string rel;
    if (!relative(path, rel)) return false;
    for (const auto& m : s.packs)
        if (m->folders.count(rel)) return true;
    return false;
}

bool read(const std::string& path, std::string& out) {
    State& s = state();
    std::lock_guard lock(s.mutex);
    const Mounted* m = nullptr;
    const Entry* e = find(path, &m);
    if (!e) return false;
    std::string stored(e->stored, '\0');
    if (fseek(m->f, static_cast<long>(e->offset), SEEK_SET) != 0 ||
        (e->stored && fread(stored.data(), 1, stored.size(), m->f) != stored.size()))
        return false;
    if (!(e->flags & kCompressed)) {
        out = std::move(stored);
        return true;
    }
    std::vector<u8> raw;
    if (!decompressBytes(reinterpret_cast<const u8*>(stored.data()), e->stored, e->size, raw)) return false;
    out.assign(reinterpret_cast<const char*>(raw.data()), raw.size());
    return true;
}

bool list(const std::string& directory, bool directories, const char* extension, std::vector<std::string>& out) {
    State& s = state();
    std::lock_guard lock(s.mutex);
    if (s.packs.empty()) return false;
    std::string rel;
    const bool atRoot = normalize(directory) == s.root;
    if (!atRoot && !relative(directory, rel)) return false;
    const std::string prefix = atRoot ? std::string() : rel + "/";
    bool found = atRoot;
    std::set<std::string> names;
    for (const auto& m : s.packs) {
        if (!atRoot && m->folders.count(rel)) found = true;
        const auto collect = [&](const std::string& p) {
            if (p.compare(0, prefix.size(), prefix) != 0) return;
            const std::string rest = p.substr(prefix.size());
            if (rest.empty() || rest.find('/') != std::string::npos) return;
            if (extension && *extension) {
                const size_t n = std::strlen(extension);
                if (rest.size() < n || rest.compare(rest.size() - n, n, extension) != 0) return;
            }
            names.insert(rest);
        };
        if (directories) for (const std::string& f : m->folders) collect(f);
        else for (const auto& [p, e] : m->entries) collect(p);
    }
    if (!found) return false;
    out.assign(names.begin(), names.end());
    return true;
}

FILE* open(const std::string& path, const char* mode) {
    if (!mode || mode[0] != 'r' || std::strchr(mode, '+')) return nullptr;
    std::string bytes;
    if (!read(path, bytes)) return nullptr;
    FILE* f = nullptr;
#if defined(_MSC_VER)
    if (tmpfile_s(&f) != 0) f = nullptr;
#else
    f = std::tmpfile();
#endif
    if (!f) return nullptr;
    if (!bytes.empty() && fwrite(bytes.data(), 1, bytes.size(), f) != bytes.size()) {
        fclose(f);
        return nullptr;
    }
    rewind(f);
    return f;
}

unsigned long long writeTime(const std::string& path) {
    State& s = state();
    std::lock_guard lock(s.mutex);
    const Mounted* m = nullptr;
    return find(path, &m) ? std::max(1ull, m->time) : 0;
}

} // namespace pack
} // namespace mealkit

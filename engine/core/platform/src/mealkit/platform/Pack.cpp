// Pack files (platform/Pack.h): writing for the cook step, mounting and lookup for platform/Paths.cpp and the OS file functions.
#include "mealkit/platform/Pack.h"

#include "mealkit/core/Compress.h"
#include "mealkit/core/Hash.h"
#include "mealkit/core/Log.h"
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
constexpr u32 kVersion = 2; // 2: chunk files (1 reads the same, without chunks)
constexpr u32 kCompressed = 1;
constexpr u32 kChunkShift = 16; // flags bits 16-31: chunk file number, 0 = in the pack

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

struct Chunk {
    std::string file;
    FILE* f = nullptr;   // opened on the first read from it
    bool failed = false; // missing or download failed: its entries read as missing
};

struct Mounted {
    std::string file;
    FILE* f = nullptr;
    std::vector<Chunk> chunks; // chunk n is chunks[n - 1]
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

// "dir/game.pak" → "dir/game" (chunk files are <stem>.<n>.chunk)
static std::string packStem(const std::string& pack) {
    return pack.size() > 4 && pack.compare(pack.size() - 4, 4, ".pak") == 0 ? pack.substr(0, pack.size() - 4) : pack;
}

std::string packChunkFile(const std::string& pack, u32 n) { return packStem(pack) + "." + std::to_string(n) + ".chunk"; }

void PackWriter::add(const std::string& path, std::string_view bytes, bool compress, u32 chunk) {
    const std::string p = normalize(path);
    for (Item& i : m_items)
        if (i.path == p) {
            i.bytes.assign(bytes);
            i.compress = compress;
            i.chunk = chunk;
            return;
        }
    m_items.push_back({p, std::string(bytes), compress, chunk});
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
    std::map<u32, std::string> chunks; // chunk number → its bytes
    std::string table;
    const auto put = [&table](const void* p, size_t n) { table.append(static_cast<const char*>(p), n); };
    for (const Item* i : sorted) {
        if (i->bytes.size() > 0xFFFFFFFFu) return error = i->path + " is over 4 GB", false;
        if (i->chunk > 0xFFFFu) return error = i->path + ": chunk number over 65535", false;
        std::string& data = i->chunk ? chunks[i->chunk] : out;
        Entry e;
        e.offset = data.size();
        e.size = static_cast<u32>(i->bytes.size());
        std::vector<u8> packed;
        if (i->compress && !i->bytes.empty())
            packed = compressBytes(reinterpret_cast<const u8*>(i->bytes.data()), e.size);
        if (!packed.empty() && packed.size() < i->bytes.size()) { // stored as is when compression doesn't help
            e.flags = kCompressed;
            e.stored = static_cast<u32>(packed.size());
            data.append(reinterpret_cast<const char*>(packed.data()), packed.size());
        } else {
            e.stored = e.size;
            data += i->bytes;
        }
        e.flags |= i->chunk << kChunkShift;
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
    // Chunk files: older ones of this pack go first (a cook with fewer chunks leaves none behind)
    const std::string dir = folder.empty() ? std::string(".") : folder;
    const std::string prefix = fileName(packStem(file)) + ".";
    for (const std::string& f : listFiles(dir, ".chunk"))
        if (f.compare(0, prefix.size(), prefix) == 0) removeFile(dir + "/" + f);
    for (const auto& [n, bytes] : chunks)
        if (!writeTextFile(packChunkFile(file, n), bytes)) return error = "cannot write " + packChunkFile(file, n), false;
    return true;
}

#if !defined(__EMSCRIPTEN__) || defined(MEALKIT_WEB_HEADLESS)
bool downloadGamePack(const std::string&, std::string&) { return true; } // the pack is next to the exe
void prefetchChunks(const std::vector<std::string>&) {}                // the chunk files are next to it too
bool fetchChunk(const std::string& file) { return fileExists(file); }
#endif

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
    if (hd.version < 1 || hd.version > kVersion) return fail("pack of another engine version (cook it again)");
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
        if (!get(&e.offset, 8) || !get(&e.stored, 4) || !get(&e.size, 4) || !get(&e.flags, 4)) return fail("damaged pack table");
        const u32 chunk = hd.version >= 2 ? e.flags >> kChunkShift : 0;
        if (hd.version < 2) e.flags &= kCompressed;
        if (chunk == 0 && e.offset + e.stored > hd.tableOffset) return fail("damaged pack table");
        if (chunk > m->chunks.size()) m->chunks.resize(chunk);
        for (size_t slash = path.find('/'); slash != std::string::npos; slash = path.find('/', slash + 1))
            m->folders.insert(path.substr(0, slash));
        m->entries.emplace(std::move(path), e);
    }
    m->hash = hd.contentHash;
    std::vector<std::string> chunkFiles;
    for (size_t n = 0; n < m->chunks.size(); ++n) chunkFiles.push_back(m->chunks[n].file = packChunkFile(file, static_cast<u32>(n + 1)));

    {
        State& s = state();
        std::lock_guard lock(s.mutex);
        if (s.packs.empty()) s.root = normalize(parentDirectory(file));
        s.packs.push_back(std::move(m));
    }
    if (!chunkFiles.empty()) prefetchChunks(chunkFiles);
    return true;
}

void unmountPacks() {
    State& s = state();
    std::lock_guard lock(s.mutex);
    for (auto& m : s.packs) {
        fclose(m->f);
        for (Chunk& c : m->chunks)
            if (c.f) fclose(c.f);
    }
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
    std::unique_lock lock(s.mutex);
    const Mounted* m = nullptr;
    const Entry* e = find(path, &m);
    if (!e) return false;
    FILE* f = m->f;
    if (const u32 n = e->flags >> kChunkShift) {
        Chunk& c = const_cast<Mounted*>(m)->chunks[n - 1];
        if (!c.f) {
            if (c.failed) return false;
            // Fetched and opened without the lock: the browser waits for the download here, and openFile asks the pack lookup
            const std::string file = c.file;
            lock.unlock();
            FILE* opened = fetchChunk(file) ? openFile(file, "rb") : nullptr;
            lock.lock();
            e = find(path, &m); // the same pack (unmounting happens only at exit)
            if (!e) {
                if (opened) fclose(opened);
                return false;
            }
            Chunk& again = const_cast<Mounted*>(m)->chunks[n - 1];
            if (again.f) {
                if (opened) fclose(opened); // another thread opened it meanwhile
            } else if (opened) {
                again.f = opened;
                MEALKIT_INFO("Pack: %s opened for %s", fileName(file).c_str(), path.c_str());
            } else {
                again.failed = true;
                MEALKIT_WARN("Pack: cannot read %s", file.c_str());
                return false;
            }
        }
        f = const_cast<Mounted*>(m)->chunks[n - 1].f;
    }
    std::string stored(e->stored, '\0');
    if (fseek(f, static_cast<long>(e->offset), SEEK_SET) != 0 || (e->stored && fread(stored.data(), 1, stored.size(), f) != stored.size()))
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

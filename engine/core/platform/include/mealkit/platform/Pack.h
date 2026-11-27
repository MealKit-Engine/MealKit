#pragma once

#include "mealkit/core/Types.h"

#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

// Pack files (docs/ROADMAP.md stage 4 "Asset cooking + pack files", docs/DECISIONS.md 2026-11-02 "Pack files"): the cook step
// (game/Cook.h) puts the shipping assets into one <name>.pak, and the shipping exe mounts it at startup (game/GameMain.cpp).
// Entries are paths relative to the pack's folder ("assets/ui/hud.ui", "mealkit.project"), each LZ4-compressed or stored.
// While a pack is mounted, the files/folders functions in platform/Paths.h serve paths under the pack's folder from the pack:
// findAsset finds only what's in a pack (no search of parent folders), and readTextFile/openFile/listFiles/fileWriteTime read it.
// So asset code keeps using plain paths; there is no separate "virtual file" API.
//
// Layout (little-endian): Header, entry data, entry table (per entry: u32 path length, path, u64 offset, u32 stored size, u32 size, u32 flags).
//
// Chunks (the browser cook, game/Cook.h `webChunks`): an entry can keep its bytes in a chunk file beside the pack, <stem>.<n>.chunk
// (flags bits 16-31 = n, offset into that file; a chunk file is only entry data). The pack's table still lists every entry, so
// isFile/list/findAsset see them all from the start; only reading one needs its chunk. Mounting starts fetching the chunk files
// (the browser downloads them in the background, platform/web/Web.cpp), and a read of a chunk not there yet waits for that one.
namespace mealkit {

// Builds a pack (cook step). Entries are written sorted by path; adding a path twice keeps the last
class PackWriter {
public:
    // chunk 0: in the pack itself; n > 0: in chunk file n (packChunkFile)
    void add(const std::string& path, std::string_view bytes, bool compress = true, u32 chunk = 0);
    // Also writes the chunk files and removes older <stem>.*.chunk files beside it
    bool write(const std::string& file, std::string& error) const;
    // Hash of all paths and uncompressed contents (build.txt "content"): the same assets give the same value on any PC
    u64 contentHash() const;
    size_t count() const { return m_items.size(); }
    u64 bytes() const; // uncompressed total

private:
    struct Item {
        std::string path;
        std::string bytes;
        bool compress = true;
        u32 chunk = 0;
    };
    std::vector<Item> m_items;
};

// Chunk file n of a pack: "dir/game.pak", 3 → "dir/game.3.chunk"
std::string packChunkFile(const std::string& pack, u32 n);

// Mounts a pack: its folder becomes the root of the files inside. Several packs can be mounted; later ones win for the same path.
// false and a reason if the file is missing or not a pack of this version
bool mountPack(const std::string& file, std::string& error);
void unmountPacks();
bool packsMounted();
// Folder of the first mounted pack (findAsset's root while packs are mounted). Empty without packs
const std::string& packRoot();
// contentHash of the mounted packs (0 without packs)
u64 mountedPackHash();
// Browser: downloads <gameName>.pak from beside the page into executableDirectory() (platform/web/Web.cpp), so the
// pack search below finds it. Elsewhere nothing to do (true)
bool downloadGamePack(const std::string& gameName, std::string& error);

// Chunk files (platform/web/Web.cpp in the browser, Pack.cpp elsewhere). Mounting a pack calls prefetchChunks with its chunk
// files in order (browser: downloads start in the background; elsewhere they are already on disk). fetchChunk makes one chunk
// file readable at its path, waiting for its download first in the browser; false if it can't
void prefetchChunks(const std::vector<std::string>& files);
bool fetchChunk(const std::string& file);

// For platform/Paths.cpp and the OS file functions. path is a full path (root + "/" + entry, either slash)
namespace pack {
bool isFile(const std::string& path);
bool isDirectory(const std::string& path);
bool read(const std::string& path, std::string& out);
// Names directly in a pack folder (files or subfolders), sorted. false if the folder isn't in a pack (then the disk is listed)
bool list(const std::string& directory, bool directories, const char* extension, std::vector<std::string>& out);
// Read modes only: a temporary FILE* holding the entry (std::tmpfile, gone on fclose) for readers that take FILE*. nullptr if not in a pack
FILE* open(const std::string& path, const char* mode);
unsigned long long writeTime(const std::string& path); // the pack file's time for its entries, 0 if not in a pack
} // namespace pack

} // namespace mealkit

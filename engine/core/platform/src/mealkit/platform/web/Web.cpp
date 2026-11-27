// Browser-only parts of the platform layer: the game pack is downloaded from beside the page into the in-memory
// file system at start (executableDirectory() is /game there), then mounted like a pack next to an exe. Its chunk files
// (platform/Pack.h "Chunks") follow in the background while the game runs; a read that needs one not there yet waits for it.
#include "mealkit/platform/Pack.h"

#include "mealkit/core/Log.h"
#include "mealkit/platform/Paths.h"

#include <cstdlib>
#include <string>

#include <emscripten/emscripten.h>

namespace mealkit {
namespace {

// Module.mealkitFiles: the downloads (JavaScript side, once).
//  fetchFile(path): streams the file of that name beside the page into the in-memory file system at path. The bytes go from the
//    response straight into one buffer that MEMFS keeps (canOwn): no copy in the wasm heap and no second copy in the file system.
//    Resolves to the size, or -1 (reason in Module.mealkitFetchError). While shown[path] is set, the page's status line
//    (Module.setStatus) shows its progress.
//  chunk(path, now): the chunk file's download, started once; now = start it even if two are already running.
//  prefetch(paths): queues chunk downloads in that order, two at a time, while the game runs.
EM_JS(void, mealkitInstallFiles, (), {
    if (Module.mealkitFiles) return;
    const status = (text) => { if (Module.setStatus) Module.setStatus(text); };
    const files = { shown: {}, started: {}, queue: [], running: 0 };
    files.fetchFile = async (path) => {
        const name = path.substring(path.lastIndexOf("/") + 1);
        try {
            const response = await fetch(name);
            if (!response.ok) {
                Module.mealkitFetchError = name + ": HTTP " + response.status;
                return -1;
            }
            // Content-Length is the size on the wire: with Content-Encoding it isn't the body's size, so then it only drives the bar
            const total = Number(response.headers.get("Content-Length")) || 0;
            const exact = total > 0 && !response.headers.get("Content-Encoding");
            let buffer = new Uint8Array(exact ? total : Math.max(total, 1 << 20));
            let got = 0;
            let shownMb = -1;
            const reader = response.body.getReader();
            for (;;) {
                const { done, value } = await reader.read();
                if (done) break;
                if (got + value.length > buffer.length) {
                    const bigger = new Uint8Array(Math.max(buffer.length * 2, got + value.length));
                    bigger.set(buffer.subarray(0, got));
                    buffer = bigger;
                }
                buffer.set(value, got);
                got += value.length;
                const mb = Math.floor(got / 1048576);
                if (files.shown[path] && mb !== shownMb) {
                    shownMb = mb;
                    status("Loading " + name + "  " + mb + (total ? " / " + Math.ceil(total / 1048576) : "") + " MB");
                }
            }
            if (got !== buffer.length) buffer = buffer.slice(0, got);
            FS.writeFile(path, buffer, { canOwn: true });
            return got;
        } catch (e) {
            Module.mealkitFetchError = name + ": " + String(e);
            return -1;
        }
    };
    const pump = () => {
        while (files.running < 2 && files.queue.length) files.chunk(files.queue.shift(), false);
    };
    files.chunk = (path, now) => {
        if (files.started[path]) return files.started[path];
        if (!now && files.running >= 2) {
            files.queue.push(path);
            return null;
        }
        files.running++;
        files.started[path] = files.fetchFile(path).then((size) => {
            files.running--;
            pump();
            return size;
        });
        return files.started[path];
    };
    files.prefetch = (paths) => {
        for (const p of paths) files.queue.push(p);
        pump();
    };
    Module.mealkitFiles = files;
});

EM_ASYNC_JS(double, mealkitFetchShown, (const char* path), {
    const p = UTF8ToString(path);
    const files = Module.mealkitFiles;
    files.shown[p] = true;
    const size = await files.fetchFile(p);
    files.shown[p] = false;
    if (Module.setStatus) Module.setStatus("");
    return size;
});

EM_JS(void, mealkitPrefetch, (const char* paths), {
    Module.mealkitFiles.prefetch(UTF8ToString(paths).split("\n").filter((p) => p.length));
});

// The chunk's download (started now if it is still queued), with its progress in the status line; a failed one is tried once more.
// The game is suspended meanwhile (JSPI), like in its frame wait
EM_ASYNC_JS(double, mealkitWaitChunk, (const char* path), {
    const p = UTF8ToString(path);
    const files = Module.mealkitFiles;
    files.shown[p] = true;
    let size = await files.chunk(p, true);
    if (size < 0) {
        delete files.started[p];
        size = await files.chunk(p, true);
    }
    files.shown[p] = false;
    if (Module.setStatus) Module.setStatus("");
    return size;
});

EM_JS(char*, mealkitFetchError, (), { return stringToNewUTF8(Module.mealkitFetchError || "unknown error"); });

std::string fetchError() {
    char* text = mealkitFetchError();
    std::string error = text;
    free(text);
    return error;
}

} // namespace

bool downloadGamePack(const std::string& gameName, std::string& error) {
    const std::string directory = executableDirectory();
    createDirectories(directory);
    const std::string path = directory + "/" + gameName + ".pak";
    mealkitInstallFiles(); // EM_JS functions are called from C++ only
    const double size = mealkitFetchShown(path.c_str());
    if (size < 0) {
        error = fetchError();
        return false;
    }
    MEALKIT_INFO("Downloaded %s.pak (%.0f KB)", gameName.c_str(), size / 1024);
    return true;
}

void prefetchChunks(const std::vector<std::string>& files) {
    std::string list;
    for (const std::string& f : files) list += f + "\n";
    mealkitInstallFiles(); // EM_JS functions are called from C++ only
    mealkitPrefetch(list.c_str());
}

bool fetchChunk(const std::string& file) {
    if (fileExists(file)) return true;
    mealkitInstallFiles(); // EM_JS functions are called from C++ only
    const double size = mealkitWaitChunk(file.c_str());
    if (size < 0) {
        MEALKIT_WARN("Cannot download %s", fetchError().c_str());
        return false;
    }
    MEALKIT_INFO("Downloaded %s (%.0f KB)", fileName(file).c_str(), size / 1024);
    return true;
}

} // namespace mealkit

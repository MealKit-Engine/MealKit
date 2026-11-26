// Browser-only parts of the platform layer: the game pack is downloaded from beside the page into the in-memory
// file system at start (executableDirectory() is /game there), then mounted like a pack next to an exe.
#include "mealkit/platform/Pack.h"

#include "mealkit/core/Log.h"
#include "mealkit/platform/Paths.h"

#include <cstdio>
#include <string>
#include <vector>

#include <emscripten/emscripten.h>

namespace mealkit {
namespace {

// Fetches url and keeps the bytes in Module.mealkitFetched; returns the size, or -1 (status in Module.mealkitFetchError)
EM_ASYNC_JS(double, mealkitFetch, (const char* url), {
    try {
        const response = await fetch(UTF8ToString(url));
        if (!response.ok) {
            Module.mealkitFetchError = "HTTP " + response.status;
            return -1;
        }
        Module.mealkitFetched = new Uint8Array(await response.arrayBuffer());
        return Module.mealkitFetched.length;
    } catch (e) {
        Module.mealkitFetchError = String(e);
        return -1;
    }
});

EM_JS(void, mealkitTakeFetched, (unsigned char* out), {
    HEAPU8.set(Module.mealkitFetched, out);
    Module.mealkitFetched = null;
});

EM_JS(char*, mealkitFetchError, (), { return stringToNewUTF8(Module.mealkitFetchError || "unknown error"); });

} // namespace

bool downloadGamePack(const std::string& gameName, std::string& error) {
    const std::string file = gameName + ".pak";
    const double size = mealkitFetch(file.c_str());
    if (size < 0) {
        char* text = mealkitFetchError();
        error = file + ": " + text;
        free(text);
        return false;
    }
    std::vector<unsigned char> bytes(static_cast<size_t>(size));
    if (!bytes.empty()) mealkitTakeFetched(bytes.data());
    const std::string directory = executableDirectory();
    createDirectories(directory);
    const std::string path = directory + "/" + file;
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f || std::fwrite(bytes.data(), 1, bytes.size(), f) != bytes.size()) {
        if (f) std::fclose(f);
        error = "cannot write " + path;
        return false;
    }
    std::fclose(f);
    MEALKIT_INFO("Downloaded %s (%zu KB)", file.c_str(), bytes.size() / 1024);
    return true;
}

} // namespace mealkit

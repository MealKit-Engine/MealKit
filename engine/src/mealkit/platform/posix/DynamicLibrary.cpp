// POSIX dynamic libraries (Linux .so, macOS .dylib): the editor's game code hot reload
#include "mealkit/platform/DynamicLibrary.h"

#include <dlfcn.h>

namespace mealkit {

void* loadLibrary(const std::string& path, std::string& error) {
    // RTLD_LOCAL: each hot reload copy keeps its own symbols (the next copy doesn't bind to the previous one's)
    void* library = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!library) {
        const char* reason = dlerror();
        error = "dlopen failed (" + std::string(reason ? reason : "unknown") + "): " + path;
    }
    return library;
}

void* findSymbol(void* library, const char* name) { return library ? dlsym(library, name) : nullptr; }

void freeLibrary(void* library) {
    if (library) dlclose(library);
}

const char* libraryExtension() {
#if defined(__APPLE__)
    return ".dylib";
#else
    return ".so";
#endif
}

} // namespace mealkit

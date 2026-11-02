#include "mealkit/platform/DynamicLibrary.h"

#include <filesystem>
#include <windows.h>

namespace mealkit {

void* loadLibrary(const std::string& path, std::string& error) {
    const std::filesystem::path wide(std::u8string(path.begin(), path.end()));
    HMODULE library = LoadLibraryW(wide.c_str());
    if (!library) error = "LoadLibrary failed (" + std::to_string(GetLastError()) + "): " + path;
    return library;
}

void* findSymbol(void* library, const char* name) {
    return library ? reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(library), name)) : nullptr;
}

void freeLibrary(void* library) {
    if (library) FreeLibrary(static_cast<HMODULE>(library));
}

const char* libraryExtension() { return ".dll"; }

} // namespace mealkit

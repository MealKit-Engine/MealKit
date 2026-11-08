#pragma once

#include <string>

// Dynamic libraries (the editor's game code hot reload): win32 LoadLibrary, posix dlopen (Linux .so, macOS .dylib).
namespace mealkit {

// nullptr on failure with the reason in error.
void* loadLibrary(const std::string& path, std::string& error);
void* findSymbol(void* library, const char* name);
void freeLibrary(void* library);
// File extension of this OS's dynamic libraries (".dll", ".so", ".dylib")
const char* libraryExtension();

} // namespace mealkit

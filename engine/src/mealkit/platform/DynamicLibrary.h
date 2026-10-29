#pragma once

#include <string>

// Dynamic libraries (the editor's game DLL hot reload). Only a Windows implementation for now (the editor is Windows-only; more in stage 5).
namespace mealkit {

// nullptr on failure with the reason in error.
void* loadLibrary(const std::string& path, std::string& error);
void* findSymbol(void* library, const char* name);
void freeLibrary(void* library);

} // namespace mealkit

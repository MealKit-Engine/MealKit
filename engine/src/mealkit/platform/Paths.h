#pragma once

#include <cstdio>
#include <string>
#include <vector>

// Files/folders (UTF-8 paths). The implementation is split into what std::filesystem can do (platform/Paths.cpp) and
// what needs OS calls (platform/win32/Paths.cpp, platform/posix/Paths.cpp).
namespace mealkit {

// Folder containing the executable (UTF-8, no trailing separator)
std::string executableDirectory();

// Searches for relative in the executable folder and its parents, and the current folder.
// During development the repo's assets folder is found even when running from build\Release\bin. Empty string if not found.
std::string findAsset(const std::string& relative);

// Asset path (relative to assets, "ui/hud.ui") → project-relative path ("assets/ui/hud.ui"). Everything that turns asset paths into files uses this.
// Source module assets "modules/<name>/ui/x.ui" are under that module's assets/: "modules/<name>/assets/ui/x.ui" (docs/MODULES.md).
// Only kind folders sit directly under assets/, so paths starting with modules/ don't collide with project assets.
std::string assetFilePath(const std::string& assetPath);
// findAsset(assetFilePath(assetPath))
std::string findAssetFile(const std::string& assetPath);

// Sets the project folder (when the general-purpose editor opens a project). Once set, findAsset searches only this folder
// (so it doesn't mistakenly pick up the engine repo's assets above the editor executable).
void setProjectRoot(const std::string& directory);
const std::string& projectRoot();

bool fileExists(const std::string& path);
bool directoryExists(const std::string& path);
std::string fileName(const std::string& path);        // just the file name from a path
std::string parentDirectory(const std::string& path); // just the folder part of a path

// Reads/writes a whole file (UTF-8 paths). Writes go to a temp file in the same folder and are swapped in, so the original isn't corrupted if power is lost midway.
bool readTextFile(const std::string& path, std::string& out);
bool writeTextFile(const std::string& path, const std::string& text);
bool createDirectories(const std::string& path);
bool removeFile(const std::string& path);
// Rename/move a file (to's folder must exist; fails if to exists)
bool moveFile(const std::string& from, const std::string& to);
// File names in a folder (excluding subfolders, sorted by name). With extension (".txt"), only that extension
std::vector<std::string> listFiles(const std::string& directory, const char* extension = nullptr);
// Subfolder names in a folder (sorted by name)
std::vector<std::string> listDirectories(const std::string& directory);
// fopen with a UTF-8 path (mode "rb", "wb" etc.). nullptr on failure.
FILE* openFile(const std::string& path, const char* mode);
// Last modified/created time (OS units, for comparison and formatFileTime; 0 if missing)
unsigned long long fileWriteTime(const std::string& path);
unsigned long long fileCreationTime(const std::string& path);
// The time above as local time "2026-10-18" or "2026-10-18 14:05". "-" for 0.
std::string formatFileTime(unsigned long long time, bool withTime);

// Base of the per-user app data folder (Windows %LOCALAPPDATA%, Linux $XDG_DATA_HOME or ~/.local/share,
// macOS ~/Library/Application Support). The engine and games make their own folders under it. The executable folder if unknown.
std::string userDataDirectory();
// User documents folder (default location for new projects). Empty string if unknown.
std::string documentsDirectory();

} // namespace mealkit

#pragma once

#include <string>

// File/folder picker dialogs (editor). Only a Windows implementation for now (editors on other OSes in stage 5).
// Paths are UTF-8. Empty string on cancel. owner is Window::nativeHandle().
namespace mealkit {

struct FileFilter {
    const char* name;     // "MealKit scene (*.scene)"
    const char* patterns; // "*.scene" (multiple as "*.glb;*.gltf")
};

std::string openFileDialog(void* owner, const std::string& initialPath, const std::string& initialFolder,
                           const FileFilter* filters, int filterCount);
// defaultExtension: appended when only a name without an extension is typed ("scene")
std::string saveFileDialog(void* owner, const std::string& initialPath, const std::string& initialFolder,
                           const FileFilter* filters, int filterCount, const char* defaultExtension);
std::string pickFolderDialog(void* owner, const std::string& initialFolder);

} // namespace mealkit

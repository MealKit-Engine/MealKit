#pragma once

#include <string>

// 파일·폴더 고르기 대화상자 (에디터). 지금은 Windows 구현만 있다 (다른 OS 에디터는 5단계).
// 경로는 UTF-8. 취소하면 빈 문자열. owner는 Window::nativeHandle().
namespace mealkit {

struct FileFilter {
    const char* name;     // "MealKit scene (*.scene)"
    const char* patterns; // "*.scene" (여러 개는 "*.glb;*.gltf")
};

std::string openFileDialog(void* owner, const std::string& initialPath, const std::string& initialFolder,
                           const FileFilter* filters, int filterCount);
// defaultExtension: 확장자 없이 이름만 적으면 붙일 것 ("scene")
std::string saveFileDialog(void* owner, const std::string& initialPath, const std::string& initialFolder,
                           const FileFilter* filters, int filterCount, const char* defaultExtension);
std::string pickFolderDialog(void* owner, const std::string& initialFolder);

} // namespace mealkit

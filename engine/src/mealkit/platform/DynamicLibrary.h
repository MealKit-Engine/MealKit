#pragma once

#include <string>

// 동적 라이브러리 (에디터의 게임 DLL 핫 리로드). 지금은 Windows 구현만 있다 (에디터가 Windows 전용, 5단계에서 더한다).
namespace mealkit {

// 실패하면 nullptr이고 error에 이유.
void* loadLibrary(const std::string& path, std::string& error);
void* findSymbol(void* library, const char* name);
void freeLibrary(void* library);

} // namespace mealkit

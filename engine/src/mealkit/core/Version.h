#pragma once

// 엔진 버전: 저장소 루트의 VERSION 파일 (CMake가 MEALKIT_VERSION으로 넘긴다).
// 게임 프로젝트의 mealkit.project engineVersion=과 비교해, 다르면 에디터가 tools\upgrade_project.ps1을 권한다.
#ifndef MEALKIT_VERSION
#define MEALKIT_VERSION "0.0.0"
#endif

namespace mealkit {

inline constexpr const char* kEngineVersion = MEALKIT_VERSION;

} // namespace mealkit

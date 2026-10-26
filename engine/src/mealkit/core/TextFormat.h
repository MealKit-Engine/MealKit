#pragma once

#include "mealkit/core/Types.h"

#include <string>
#include <string_view>

// MealKit 텍스트 형식의 형식 줄 (docs/CODING_MODEL.md "텍스트 파일 형식이 바뀔 때", 도구 사이 약속).
// 모든 형식은 주석 머리 다음 첫 줄이 "@format <종류> <N>"이다:
//   # MealKit table (SoundEvent). Edit it in the editor's [Tables] panel or import a CSV there.
//   @format table 1
// 줄이 없으면 버전 1. 쓰는 쪽은 항상 최신 버전으로 쓰고, 형식을 바꾸면 N을 올리고 읽는 쪽에 migrate를 더한다.
// 다른 저장소의 MealKit 도구도 이 줄로 파일을 알아본다(확장자가 같은 sounds.table·input.bindings도).
namespace mealkit::text {

struct Format {
    const char* kind;
    u32 version; // 이 엔진이 쓰는 최신 버전
};

inline constexpr Format kProject{"project", 1};   // mealkit.project
inline constexpr Format kTuning{"tuning", 1};     // game.tuning, view.tuning
inline constexpr Format kSettings{"settings", 1}; // 플레이어 설정 settings.ini
inline constexpr Format kTable{"table", 1};       // *.table, input.bindings, feedback.table, sounds.table
inline constexpr Format kScene{"scene", 2};       // *.scene (버전 2: 부모 기준 값. 예전 "version N" 줄도 읽는다)
inline constexpr Format kUi{"ui", 1};             // *.ui, theme.ui
inline constexpr Format kStrings{"strings", 1};   // strings/<언어>.txt
inline constexpr Format kTool{"tool", 1};         // 도구 매니페스트 mealkit-tool.txt

// "@format <종류> <N>" (줄바꿈 없음)
std::string formatLine(const Format& format);

// line(앞뒤 공백을 뺀 줄)이 "@"로 시작하면 형식 줄로 읽고 true. version에 파일의 버전을 넣는다.
// 종류가 다르거나, 이 엔진보다 새 버전이거나, 모양이 틀리면 path와 함께 경고한다(새 버전이면 아는 키만 읽는다).
bool readFormatLine(std::string_view line, const Format& expected, const std::string& path, u32& version);

} // namespace mealkit::text

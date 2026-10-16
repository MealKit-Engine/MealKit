#pragma once

#include "mealkit/core/Reflect.h"

// 게임이 더하는 프로젝트 설정: mealkit.project의 game.* 줄 (에디터 [파일] > [프로젝트 설정]에서 고친다).
// 게임 실행 파일이 시작할 때 한 번 읽는다. 게임 규칙 값은 여기가 아니라 Tuning(game.tuning)에 둔다.
// 콘텐츠 해시에서 뺄 항목(시뮬레이션에 닿지 않는 값)은 MEALKIT_VIEW_FIELD로 선언한다.
namespace sandbox {

MEALKIT_STRUCT(ProjectSettings) {
    MEALKIT_VIEW_FIELD(mealkit::u16, defaultPort, 27015, "기본 UDP 포트 (--host, --join)");
    MEALKIT_VIEW_FIELD(mealkit::u32, defaultMaxPlayers, 4, "방을 만들 때 기본 최대 인원 (최대 8)");
};

} // namespace sandbox

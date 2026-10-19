#pragma once

#include "mealkit/core/Reflect.h"
#include "mealkit/core/Types.h"

#include <string>

// 진행 데이터 (mealkit/game/Save.h): 사용자 폴더의 saves/progress.save. 매치 밖에 남는 기록만, 시뮬레이션은 읽지 않는다.
// 필드를 더하거나 지워도 예전 파일을 읽는다(없는 필드는 기본값). 뜻이 바뀌면 kSaveVersion을 올리고 migrate에서 옮긴다.
namespace sandbox {

inline constexpr mealkit::u32 kSaveVersion = 1;

MEALKIT_STRUCT(SaveData) {
    MEALKIT_FIELD(mealkit::u32, matchesStarted, 0, "시작한 매치 수");
    MEALKIT_FIELD(mealkit::u32, falls, 0, "내가 떨어진 횟수 (모든 매치)");
    MEALKIT_FIELD(mealkit::u32, pushes, 0, "내 밀치기 수 (모든 매치)");
    MEALKIT_FIELD(mealkit::f32, playSeconds, 0.0f, "플레이 시간 (초)");
    MEALKIT_FIELD(mealkit::u32, body, 0, "고른 몸 종류 (bodies.table 행 번호, 다음 매치의 로드아웃)");
    MEALKIT_FIELD(std::string, playerName, {}, "로비에 보이는 내 이름 (비어 있으면 계정·사용자 이름)");
};

} // namespace sandbox

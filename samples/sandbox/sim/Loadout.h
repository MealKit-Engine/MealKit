#pragma once

#include "mealkit/core/Reflect.h"
#include "mealkit/core/Types.h"

// 매치 시작 데이터 (mealkit/game/Loadout.h): 플레이어마다 매치 전에 고르고, 호스트가 시작할 때 확정해 모두에게 보낸다.
// 매치 시작(Headless::startWorld)과 중간 참가 명령(sim/Commands.cpp)이 플레이어 슬롯(상태)에 복사하고, 시뮬레이션은 그 복사본만 읽는다. memcpy로 보내므로 문자열 없음.
namespace sandbox {

MEALKIT_STRUCT(Loadout) {
    MEALKIT_FIELD(mealkit::u32, body, 0, "몸 종류 (tables/bodies.table 행 번호)");
};

} // namespace sandbox

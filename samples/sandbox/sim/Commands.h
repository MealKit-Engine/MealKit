#pragma once

#include "mealkit/core/Reflect.h"
#include "mealkit/core/Types.h"

// 확정 명령 타입 (mealkit/sim/Commands.h): 화면 쪽이 sim::sendCommand로 보내고, World::step() 맨 앞에서 applyCommands가 처리한다.
// memcpy로 보내므로 문자열·컨테이너를 넣지 않는다.
namespace sandbox {

// 디버그: 보낸 플레이어의 캐릭터 주변 상자를 밀어낸다 (밀치기와 같은 힘, 재사용 대기 없음). 리스폰 대기 중이면 아무 일도 없다.
MEALKIT_STRUCT(DebugShockwave) {
    MEALKIT_FIELD(mealkit::u32, pushes, 1, "밀치기 횟수");
};

} // namespace sandbox

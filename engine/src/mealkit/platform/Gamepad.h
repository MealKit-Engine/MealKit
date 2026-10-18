#pragma once

#include "mealkit/core/Types.h"
#include "mealkit/input/Actions.h"

// 게임패드 (플랫폼 계층). Windows는 XInput(Xbox·XInput 호환 패드, 슬롯 4개).
// 연결되지 않은 슬롯은 읽기가 느려서(슬롯당 약 14 µs, docs/DECISIONS.md 2026-10-18) 1초에 한 번만 본다.
// 엔진(출시 exe, 에디터 플레이, 참가 창)이 하나 만들어 매 프레임 update하고, 첫 번째 연결된 패드를 입력 액션에 넘긴다.
namespace mealkit {

class Gamepads {
public:
    static constexpr u32 kSlots = 4;

    // 매 프레임 한 번. dt는 초 (연결 확인 간격을 센다)
    void update(f64 dt);
    const input::PadState& state(u32 slot) const { return m_pads[slot]; }
    // 연결된 첫 패드 (없으면 연결 안 됨 상태). 한 PC 한 플레이어 게임이 쓴다
    const input::PadState& first() const;
    u32 connectedCount() const;

private:
    input::PadState m_pads[kSlots];
    f64 m_untilCheck[kSlots] = {}; // 연결 안 된 슬롯: 다음 확인까지 남은 초
};

} // namespace mealkit

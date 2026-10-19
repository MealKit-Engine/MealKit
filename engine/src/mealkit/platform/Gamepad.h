#pragma once

#include "mealkit/core/Types.h"

// 게임패드 (플랫폼 계층). Windows는 XInput(Xbox·XInput 호환 패드, 슬롯 4개).
// 연결되지 않은 슬롯은 읽기가 느려서(슬롯당 약 14 µs, docs/DECISIONS.md 2026-10-18) 1초에 한 번만 본다.
// 엔진(출시 exe, 에디터 플레이, 참가 창)이 하나 만들어 매 프레임 update하고, 첫 번째 연결된 패드를 입력 액션에 넘긴다.
namespace mealkit {

// 게임패드 한 개의 상태 (Gamepads가 채우고 입력 액션 input::Actions가 읽는다). 스틱 y는 위가 +.
namespace pad {
inline constexpr u16 A = 1u << 0, B = 1u << 1, X = 1u << 2, Y = 1u << 3;
inline constexpr u16 LeftShoulder = 1u << 4, RightShoulder = 1u << 5, Back = 1u << 6, Start = 1u << 7;
inline constexpr u16 LeftThumb = 1u << 8, RightThumb = 1u << 9;
inline constexpr u16 DPadUp = 1u << 10, DPadDown = 1u << 11, DPadLeft = 1u << 12, DPadRight = 1u << 13;
} // namespace pad

struct PadState {
    bool connected = false;
    u16 buttons = 0; // pad::A ...
    f32 leftX = 0.0f, leftY = 0.0f, rightX = 0.0f, rightY = 0.0f; // -1..1 (데드존 전)
    f32 leftTrigger = 0.0f, rightTrigger = 0.0f;                 // 0..1
};

class Gamepads {
public:
    static constexpr u32 kSlots = 4;

    // 매 프레임 한 번. dt는 초 (연결 확인 간격을 센다)
    void update(f64 dt);
    const PadState& state(u32 slot) const { return m_pads[slot]; }
    // 연결된 첫 패드 (없으면 연결 안 됨 상태). 한 PC 한 플레이어 게임이 쓴다
    const PadState& first() const;
    u32 connectedCount() const;

private:
    PadState m_pads[kSlots];
    f64 m_untilCheck[kSlots] = {}; // 연결 안 된 슬롯: 다음 확인까지 남은 초
};

} // namespace mealkit

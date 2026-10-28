#pragma once

#include "cooldown/sim/Cooldown.h"

// 쿨타임 화면용 값. 뷰 모델에 넣어 assets/ui/cooldown_bar.ui의 widthRatio 등에 묶는다.
namespace cooldown {

// 0(막 썼음) .. 1(준비됨)
inline mealkit::f32 readiness(const Cooldown& c, u32 tick) {
    if (c.length == 0) return 1.0f;
    const u32 left = remaining(c, tick);
    return 1.0f - static_cast<mealkit::f32>(left) / static_cast<mealkit::f32>(c.length);
}

// 남은 시간 (초, 화면 글자용)
inline mealkit::f32 secondsLeft(const Cooldown& c, u32 tick) {
    return static_cast<mealkit::f32>(remaining(c, tick)) / static_cast<mealkit::f32>(mealkit::reflect::tickRate());
}

} // namespace cooldown

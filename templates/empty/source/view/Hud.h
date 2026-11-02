#pragma once

#include "sim/World.h"
#include "view/ViewTuning.h"

#include "mealkit/core/Reflect.h"

#include <string>

// View models for game screen UI (mealkit/ui/Ui.h). The view side fills state → view model every frame, and assets/ui/*.ui binds by field name.
namespace sandbox {

MEALKIT_STRUCT(ChatLineVm) {
    MEALKIT_FIELD(std::string, name, {}, "보낸 사람");
    MEALKIT_FIELD(std::string, text, {}, "내용");
};

// assets/ui/hud.ui. The chat fields are filled by the screen flow (app/Flow.cpp)
MEALKIT_VIEWMODEL(HudVm) {
    MEALKIT_FIELD(mealkit::f32, time, 0.0f, "남은 경기 시간 (초, 끝없는 경기면 지난 시간)");
    MEALKIT_FIELD(mealkit::u32, countdown, 0, "시작 카운트다운 (초, 0이면 없음)");
    MEALKIT_LIST(ChatLineVm, chat, 5, "채팅 (최근, 잠시 보인다)");
    MEALKIT_FIELD(bool, chatOpen, false, "채팅을 치는 중");
    MEALKIT_FIELD(std::string, chatDraft, {}, "보낼 채팅 (글자 입력)");
};

// State → HUD (every frame)
void fillHud(const WorldState& state, const Tuning& tuning, HudVm& vm);

} // namespace sandbox

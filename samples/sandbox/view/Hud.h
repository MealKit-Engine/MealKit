#pragma once

#include "sim/World.h"
#include "view/ViewTuning.h"

#include "mealkit/core/Reflect.h"

#include <array>
#include <string>

// 게임 화면 UI의 뷰 모델 (mealkit/ui/Ui.h). 화면 쪽이 매 프레임 상태 → 뷰 모델을 채우고, assets/ui/*.ui가 필드 이름으로 바인딩한다.
// UI는 이 값을 읽기만 한다 (버튼은 액션을 내고, 시뮬레이션을 바꾸는 일은 GameSession이 확정 명령으로 보낸다).
namespace sandbox {

MEALKIT_STRUCT(PlayerRowVm) {
    MEALKIT_FIELD(std::string, name, {}, "이름");
    MEALKIT_FIELD(mealkit::u32, falls, 0, "떨어진 횟수");
    MEALKIT_FIELD(bool, you, false, "내 줄");
    MEALKIT_FIELD(mealkit::Color, color, {}, "플레이어 색");
};

MEALKIT_VIEWMODEL(HudVm) {
    MEALKIT_FIELD(mealkit::f32, time, 0.0f, "경기 시간 (초)");
    MEALKIT_FIELD(mealkit::f32, pushReady, 1.0f, "밀치기 준비 (0..1)");
    MEALKIT_FIELD(bool, alive, true, "내 캐릭터가 있다");
    MEALKIT_LIST(PlayerRowVm, players, mealkit::net::kMaxPlayers, "플레이어");
};

MEALKIT_VIEWMODEL(NameTagVm) {
    MEALKIT_FIELD(std::string, name, {}, "이름");
    MEALKIT_FIELD(bool, you, false, "나");
    MEALKIT_FIELD(mealkit::Color, color, {}, "플레이어 색");
};

// 상태 → HUD (매 프레임)
void fillHud(const WorldState& state, const Tuning& tuning, u8 localPlayer, const ViewTuning& look, HudVm& vm);

} // namespace sandbox

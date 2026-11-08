#pragma once

#include "sim/World.h"
#include "view/ViewTuning.h"

#include "mealkit/core/Reflect.h"

#include <array>
#include <string>

// View models for game screen UI (mealkit/ui/Ui.h). The view side fills state → view model every frame, and assets/ui/*.ui binds by field name.
// The UI only reads these values (buttons fire actions; changes to the simulation are sent by GameSession as confirmed commands).
namespace coop {

MEALKIT_STRUCT(PlayerRowVm) {
    MEALKIT_FIELD(std::string, name, {}, "이름");
    MEALKIT_FIELD(mealkit::u32, falls, 0, "쓰러진 횟수");
    MEALKIT_FIELD(bool, you, false, "내 줄");
    MEALKIT_FIELD(mealkit::Color, color, {}, "플레이어 색");
    MEALKIT_FIELD(mealkit::u32, ping, 0, "호스트와의 왕복 시간 (ms)");
    MEALKIT_FIELD(bool, pingGood, false, "핑을 보인다 (좋음)");
    MEALKIT_FIELD(bool, pingBad, false, "핑을 보인다 (나쁨)");
};

MEALKIT_STRUCT(ChatLineVm) {
    MEALKIT_FIELD(std::string, name, {}, "보낸 사람");
    MEALKIT_FIELD(std::string, text, {}, "내용");
};

MEALKIT_VIEWMODEL(HudVm) {
    MEALKIT_FIELD(mealkit::f32, time, 0.0f, "남은 경기 시간 (초, 끝없는 경기면 지난 시간)");
    MEALKIT_FIELD(mealkit::u32, countdown, 0, "시작 카운트다운 (초, 0이면 없음)");
    MEALKIT_FIELD(bool, go, false, "막 시작했다 (시작! 표시)");
    MEALKIT_FIELD(mealkit::f32, pushReady, 1.0f, "밀치기 준비 (0..1)");
    MEALKIT_FIELD(bool, alive, true, "내 캐릭터가 있다");
    MEALKIT_FIELD(bool, downed, false, "내가 쓰러져 있다");
    MEALKIT_FIELD(mealkit::u32, revive, 0, "부활 진행 (%)");
    MEALKIT_FIELD(mealkit::u32, goalMeters, 0, "목표까지 거리 (m)");
    MEALKIT_FIELD(bool, showGoal, false, "목표 거리를 보인다");
    MEALKIT_FIELD(bool, won, false, "성공 (라운드 끝)");
    MEALKIT_FIELD(bool, lost, false, "실패 (라운드 끝)");
    MEALKIT_FIELD(mealkit::u32, totalFalls, 0, "기록: 모든 매치에서 쓰러진 횟수 (세이브)");
    MEALKIT_FIELD(mealkit::u32, playMinutes, 0, "기록: 플레이 시간 (분, 세이브)");
    MEALKIT_LIST(PlayerRowVm, players, mealkit::session::kMaxPlayers, "플레이어");
    MEALKIT_LIST(ChatLineVm, chat, 5, "채팅 (최근, 잠시 보인다)");
    MEALKIT_FIELD(bool, chatOpen, false, "채팅을 치는 중");
    MEALKIT_FIELD(std::string, waiting, {}, "입력을 기다리는 플레이어 (멈췄을 때, 플레이어 설정으로 끈다)");
    MEALKIT_FIELD(std::string, chatDraft, {}, "보낼 채팅 (글자 입력)");
};

MEALKIT_VIEWMODEL(NameTagVm) {
    MEALKIT_FIELD(std::string, name, {}, "이름");
    MEALKIT_FIELD(bool, you, false, "나");
    MEALKIT_FIELD(mealkit::Color, color, {}, "플레이어 색");
};

// State → HUD (every frame). names: a name per player index (set in the lobby)
void fillHud(const World& world, u8 localPlayer, const ViewTuning& look,
             const std::array<std::string, mealkit::session::kMaxPlayers>& names, HudVm& vm);

} // namespace coop

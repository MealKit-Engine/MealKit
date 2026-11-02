#pragma once

#include "view/Hud.h"

#include "mealkit/core/Reflect.h"
#include "mealkit/net/Lockstep.h"

#include <string>

// View models of the screen flow (title → menu → join → lobby → loading → match (pause, results) → lobby). assets/ui/*.ui binds by field name.
// Which screen to open and button handling are in app/Flow.cpp. Screens only read these values; only input widgets (name, address, chat, ready, choice) write their own fields.
namespace sandbox {

MEALKIT_VIEWMODEL(TitleVm) {
    MEALKIT_FIELD(std::string, version, {}, "엔진 버전");
};

MEALKIT_VIEWMODEL(MenuVm) {
    MEALKIT_FIELD(std::string, playerName, {}, "내 이름 (글자 입력, 로비에 보인다)");
    MEALKIT_FIELD(bool, onlineAvailable, false, "온라인(Steam) 방을 만들 수 있다");
    MEALKIT_FIELD(std::string, error, {}, "방금 실패한 이유");
};

MEALKIT_STRUCT(RoomRowVm) {
    MEALKIT_FIELD(std::string, hostName, {}, "방장 이름");
    MEALKIT_FIELD(std::string, players, {}, "인원 (2/4)");
    MEALKIT_FIELD(std::string, address, {}, "주소");
    MEALKIT_FIELD(bool, compatible, true, "같은 버전 (아니면 들어갈 수 없다)");
    MEALKIT_FIELD(std::string, note, {}, "들어갈 수 없는 이유 (버전이 다름, 데이터가 다름)");
};

MEALKIT_VIEWMODEL(JoinVm) {
    MEALKIT_FIELD(std::string, address, {}, "주소 (글자 입력)");
    MEALKIT_LIST(RoomRowVm, rooms, 12, "같은 네트워크의 방");
    MEALKIT_FIELD(bool, noRooms, true, "찾은 방이 없다");
    MEALKIT_FIELD(std::string, error, {}, "방금 실패한 이유");
};

MEALKIT_STRUCT(LobbyRowVm) {
    MEALKIT_FIELD(std::string, name, {}, "이름");
    MEALKIT_FIELD(std::string, choice, {}, "로비에서 고른 것 (비면 없음)");
    MEALKIT_FIELD(bool, ready, false, "준비");
    MEALKIT_FIELD(bool, host, false, "방장");
    MEALKIT_FIELD(bool, you, false, "나");
    MEALKIT_FIELD(mealkit::Color, color, {}, "플레이어 색");
    MEALKIT_FIELD(std::string, ping, {}, "호스트와의 왕복 시간 (\"45 ms\", 방장·혼자는 비어 있다)");
};

MEALKIT_VIEWMODEL(LobbyVm) {
    MEALKIT_FIELD(std::string, title, {}, "방 이름 (혼자 / 주소 / Steam)");
    MEALKIT_FIELD(bool, connecting, false, "아직 접속 중");
    MEALKIT_FIELD(bool, isHost, false, "내가 방장");
    MEALKIT_FIELD(bool, offline, false, "혼자 하기");
    MEALKIT_FIELD(bool, online, false, "Steam 로비 (친구 초대)");
    MEALKIT_FIELD(bool, ready, false, "내 준비 (토글)");
    MEALKIT_FIELD(bool, hasChoice, false, "로비에서 고를 것이 있다 (FlowGame::choices)");
    MEALKIT_FIELD(mealkit::u32, choice, 0, "내가 고른 것 (선택, FlowGame::choices 번호)");
    MEALKIT_FIELD(bool, canStart, false, "시작할 수 있다 (모두 준비)");
    MEALKIT_FIELD(bool, waiting, false, "방장이 시작하기를 기다린다");
    MEALKIT_FIELD(bool, needReady, false, "방장: 모두 준비하기를 기다린다");
    MEALKIT_FIELD(std::string, slowWarning, {}, "핑이 입력 지연 상한보다 큰 사람 (매치가 멈칫거린다)");
    MEALKIT_FIELD(std::string, version, {}, "게임 버전 (v0.1.0 (커밋))");
    MEALKIT_LIST(LobbyRowVm, players, mealkit::net::kMaxPlayers, "플레이어");
    MEALKIT_LIST(ChatLineVm, chat, 8, "채팅 (최근)");
    MEALKIT_FIELD(std::string, chatDraft, {}, "보낼 채팅 (글자 입력)");
};

MEALKIT_VIEWMODEL(PauseVm) {
    MEALKIT_FIELD(bool, offline, false, "혼자 하기 (게임이 멈춘다)");
    MEALKIT_FIELD(bool, isHost, false, "방장 (로비로 돌아갈 수 있다)");
};

MEALKIT_STRUCT(ResultRowVm) {
    MEALKIT_FIELD(mealkit::u32, rank, 1, "순위");
    MEALKIT_FIELD(std::string, name, {}, "이름");
    MEALKIT_FIELD(mealkit::u32, score, 0, "점수 (게임이 정한 수, FlowGame::score)");
    MEALKIT_FIELD(bool, you, false, "나");
    MEALKIT_FIELD(bool, winner, false, "1위");
    MEALKIT_FIELD(mealkit::Color, color, {}, "플레이어 색");
};

MEALKIT_VIEWMODEL(ResultsVm) {
    MEALKIT_LIST(ResultRowVm, rows, mealkit::net::kMaxPlayers, "순위");
    MEALKIT_FIELD(bool, isHost, false, "방장 (로비로 버튼)");
    MEALKIT_FIELD(mealkit::u32, secondsLeft, 0, "자동으로 로비로 돌아가기까지 (방장)");
};

MEALKIT_VIEWMODEL(MessageVm) {
    MEALKIT_FIELD(std::string, title, {}, "제목 (끊김 / 들어갈 수 없음)");
    MEALKIT_FIELD(std::string, text, {}, "알림 (끊긴 이유)");
};

} // namespace sandbox

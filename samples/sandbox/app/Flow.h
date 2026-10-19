#pragma once

#include "app/SaveData.h"
#include "app/Screens.h"
#include "sim/World.h"
#include "view/Hud.h"

#include "mealkit/game/GameContext.h"
#include "mealkit/ui/Ui.h"

#include <string>

// 화면 흐름 (ROADMAP 1단계 "기본 화면 흐름 템플릿"): 타이틀 → 메뉴 → (혼자 / 방 만들기 / 참가: LAN 방·주소 / Steam) → 로비
// (이름·준비·몸·채팅) → 로딩 → 매치(HUD, 채팅, 일시정지) → 결과 → 로비. 끊기면 알림 → 메뉴.
// 어느 화면인지는 세션 단계(ctx.match.phase())와 시뮬레이션 단계(MatchPhase)가 정하고, 이 클래스는 화면을 열고 닫고 버튼을 처리한다.
// 세션을 열고 닫는 일은 ctx.match(엔진의 MatchService)만 한다. 시뮬레이션은 바꾸지 않는다 (매치 끝은 sim/Match.cpp가 정한다).
// 게임마다 이 파일과 assets/ui/의 화면 .ui를 고쳐 쓴다.
namespace sandbox {

enum class Screen : mealkit::u8 { None, Title, Menu, Join, Lobby, Loading, Match, Message };

class Flow {
public:
    // 매 프레임 (게임 update 맨 앞). world는 매치 중이면 지금 상태, 아니면 nullptr. hud·hudScreen은 매치 중 채팅 줄
    void update(mealkit::game::GameContext& ctx, const World* world, SaveData& save, HudVm& hud,
                mealkit::ui::ScreenId hudScreen, mealkit::f64 dt);
    // 매치가 끝났다 (GameInstance::stop): 일시정지·결과 화면을 닫는다
    void stop(mealkit::game::GameContext& ctx);

    Screen screen() const { return m_screen; }
    bool blocksInput() const;        // 일시정지·채팅·결과 중: 내 캐릭터에 입력을 보내지 않고 카메라도 돌리지 않는다
    bool pausesSimulation() const;   // 혼자 하기의 일시정지 (여러 명이면 매치는 계속된다)
    bool takeSaveChanged();          // 이름·몸을 바꿨다 (세이브를 쓴다)

    // 플레이어 번호의 이름 (로비에서 정한 이름, 없으면 P1..)
    static std::string playerName(const mealkit::net::LockstepSession& session, mealkit::u32 slot);

private:
    void show(mealkit::game::GameContext& ctx, Screen screen);
    void closeAll(mealkit::game::GameContext& ctx);
    void updateMenu(mealkit::game::GameContext& ctx, SaveData& save);
    void updateJoin(mealkit::game::GameContext& ctx);
    void updateLobby(mealkit::game::GameContext& ctx, SaveData& save);
    void updateMatch(mealkit::game::GameContext& ctx, const World& world, HudVm& hud, mealkit::ui::ScreenId hudScreen,
                     mealkit::f64 dt);
    void fillChat(const mealkit::net::LockstepSession& session, ChatLineVm* lines, mealkit::u32& count, mealkit::u32 capacity);
    void fillResults(mealkit::game::GameContext& ctx, const World& world);

    Screen m_screen = Screen::None;
    Screen m_menuScreen = Screen::Title; // 세션이 없을 때 (타이틀, 메뉴, 참가)
    struct Ids {
        mealkit::ui::ScreenId title = 0, menu = 0, join = 0, lobby = 0, loading = 0, message = 0, pause = 0, results = 0;
    } m_ids;
    TitleVm m_title;
    MenuVm m_menu;
    JoinVm m_join;
    LobbyVm m_lobby;
    PauseVm m_pause;
    ResultsVm m_results;
    MessageVm m_message;
    bool m_nameLoaded = false;
    bool m_saveChanged = false;
    bool m_paused = false;
    bool m_chatOpen = false;
    bool m_chatFocusPending = false;
    mealkit::f64 m_resultsTime = 0.0;
    mealkit::f64 m_time = 0.0;
    mealkit::f64 m_chatShownUntil = 0.0; // 매치 중 HUD의 채팅 줄을 보여 줄 때까지 (새 줄이 오면 늘어난다)
    mealkit::u32 m_lastChatSerial = 0;
};

} // namespace sandbox

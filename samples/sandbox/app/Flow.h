#pragma once

#include "app/SaveData.h"
#include "app/Screens.h"
#include "sim/World.h"
#include "view/Hud.h"

#include "mealkit/game/GameContext.h"
#include "mealkit/ui/Ui.h"

#include <string>

// Screen flow (ROADMAP stage 1 "Basic screen flow template"): title → menu → (solo / create room / join: LAN room or address / Steam) → lobby
// (name/ready/body/chat) → loading → match (HUD, chat, pause) → results → lobby. On disconnect, a notice → menu.
// Which screen is decided by the session phase (ctx.match.phase()) and the simulation phase (MatchPhase); this class opens/closes screens and handles buttons.
// Only ctx.match (the engine's MatchService) opens and closes sessions. The simulation isn't changed (match end is decided by sim/Match.cpp).
// Each game edits this file and the screen .ui files in assets/ui/.
namespace sandbox {

enum class Screen : mealkit::u8 { None, Title, Menu, Join, Lobby, Loading, Match, Message };

class Flow {
public:
    // Every frame (at the very start of game update). world is the current state mid-match, otherwise nullptr. hud/hudScreen are the chat line mid-match
    void update(mealkit::game::GameContext& ctx, const World* world, SaveData& save, HudVm& hud,
                mealkit::ui::ScreenId hudScreen, mealkit::f64 dt);
    // The match ended (GameInstance::stop): close the pause/results screens
    void stop(mealkit::game::GameContext& ctx);

    Screen screen() const { return m_screen; }
    bool blocksInput() const;        // during pause/chat/results: no input to my character and the camera doesn't rotate either
    bool pausesSimulation() const;   // pause in solo play (with multiple players the match continues)
    bool takeSaveChanged();          // the name/body changed (write the save)

    // Name of a player index (name set in the lobby, P1.. if none)
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
    std::string endText(mealkit::game::GameContext& ctx) const;

    Screen m_screen = Screen::None;
    Screen m_menuScreen = Screen::Title; // when there's no session (title, menu, join)
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
    std::string m_version; // "v0.1.0 (commit)"
    bool m_nameLoaded = false;
    bool m_saveChanged = false;
    bool m_paused = false;
    bool m_chatOpen = false;
    bool m_chatFocusPending = false;
    mealkit::f64 m_resultsTime = 0.0;
    mealkit::f64 m_time = 0.0;
    mealkit::f64 m_chatShownUntil = 0.0; // until when to show the HUD chat line mid-match (extended when new lines arrive)
    mealkit::u32 m_lastChatSerial = 0;
};

} // namespace sandbox

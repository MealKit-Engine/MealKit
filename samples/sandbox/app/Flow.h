#pragma once

#include "app/SaveData.h"
#include "app/Screens.h"
#include "view/Hud.h"

#include "mealkit/game/GameContext.h"
#include "mealkit/ui/Ui.h"

#include <array>
#include <span>
#include <string>

// Screen flow (ROADMAP stage 1 "Basic screen flow template"): title → menu → (solo / create room / join: LAN room or address / Steam) → lobby
// (name/ready/body/chat) → loading → match (HUD, chat, pause) → results → lobby. On disconnect, a notice → menu.
// Which screen is decided by the session phase (ctx.match.phase()) and the simulation phase (MatchPhase); this class opens/closes screens and handles buttons.
// Only ctx.match (the engine's MatchService) opens and closes sessions. The simulation isn't changed (match end is decided by sim/Match.cpp).
// Flow.h, Flow.cpp, Screens.h and the flow screens in assets/ui/ are the same in every project template (tools\new_project.ps1 copies
// them from samples/sandbox). The game's own parts come in through FlowGame, and the game's SaveData (playerName, lobbyChoice) and
// HudVm (chat, chatCount, chatOpen, chatDraft) have the fields this file uses. Each game may edit all of it.
namespace sandbox {

// What the game tells the screen flow every frame (GameSession fills it)
struct FlowGame {
    std::span<const mealkit::Color> playerColors; // per player index (lobby, results)
    mealkit::f32 resultsSeconds = 20.0f;          // results → the host returns everyone to the lobby after this
    mealkit::f32 chatShowSeconds = 8.0f;          // new chat lines stay on the HUD this long during a match
    // Lobby choice (match start data): option string keys, the same keys as the select box in ui/lobby.ui. Empty = no choice (hidden)
    std::span<const char* const> choices;
    mealkit::u32 (*memberChoice)(const mealkit::net::LobbyMember& member) = nullptr; // another player's choice (from their loadout)
    // The match (only while the simulation runs)
    bool inMatch = false;
    bool results = false; // the simulation reached its results phase (same tick for everyone)
    mealkit::u32 playerCount = 0;
    std::array<mealkit::u32, mealkit::net::kMaxPlayers> score{}; // results rows: one number per player
    bool lowestFirst = false;                                    // rank by the lowest score (falls) instead of the highest
};

enum class Screen : mealkit::u8 { None, Title, Menu, Join, Lobby, Loading, Match, Message };

class Flow {
public:
    // Every frame (at the very start of game update). hud/hudScreen are the chat line mid-match
    void update(mealkit::game::GameContext& ctx, const FlowGame& game, SaveData& save, HudVm& hud,
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
    void updateLobby(mealkit::game::GameContext& ctx, const FlowGame& game, SaveData& save);
    void updateMatch(mealkit::game::GameContext& ctx, const FlowGame& game, HudVm& hud, mealkit::ui::ScreenId hudScreen);
    void fillChat(const mealkit::net::LockstepSession& session, ChatLineVm* lines, mealkit::u32& count, mealkit::u32 capacity);
    void fillResults(mealkit::game::GameContext& ctx, const FlowGame& game);
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

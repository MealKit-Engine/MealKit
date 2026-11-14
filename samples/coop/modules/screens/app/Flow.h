#pragma once

#include "screens/app/Screens.h"

#include "mealkit/game/GameContext.h"
#include "mealkit/ui/Ui.h"

#include <array>
#include <span>
#include <string>

// Screen flow (docs/index.md): title → menu → (solo / create room / join: LAN room or address / Steam) → lobby
// (name/ready/choice/chat) → loading → match (chat, pause) → results → lobby. On disconnect, a notice → menu.
// Which screen is decided by the session phase (ctx.match.phase()) and what the game reports (FlowGame::inMatch/results);
// this class opens/closes screens and handles buttons. Only ctx.match (the engine's MatchService) opens and closes sessions,
// and the simulation isn't changed (the game's sim/ decides the end of a match).
// Screens open from modules/screens/ui/<name>.ui, or from the game's assets/ui/<name>.ui when it has its own copy.
namespace screens {

// What the game tells the screen flow every frame (its GameInstance fills it)
struct FlowGame {
    std::span<const mealkit::Color> playerColors; // per player index (lobby, results)
    mealkit::f32 resultsSeconds = 20.0f;          // results → the host returns everyone to the lobby after this
    mealkit::f32 chatShowSeconds = 8.0f;          // new chat lines stay on the HUD this long during a match
    // Lobby choice (match start data): string keys of the options, the same keys as the select box in ui/lobby.ui
    // (the module's lobby.ui uses lobby.option1..3). Empty = no choice (hidden)
    std::span<const char* const> choices;
    mealkit::u32 (*memberChoice)(const mealkit::session::LobbyMember& member) = nullptr; // another player's choice (from their loadout)
    // The match (only while the simulation runs)
    bool inMatch = false;
    bool results = false; // the simulation reached its results phase (same tick for everyone)
    mealkit::u32 playerCount = 0;
    std::array<mealkit::u32, mealkit::session::kMaxPlayers> score{}; // results rows: one number per player
    bool lowestFirst = false;                                    // rank by the lowest score instead of the highest
};

// The game's saved values the flow reads and writes (point into the game's own save struct; nullptr = not saved)
struct FlowSave {
    std::string* playerName = nullptr;   // name shown in the lobby (empty = account/user name)
    mealkit::u32* lobbyChoice = nullptr; // FlowGame::choices index picked last
};
template <class Save> FlowSave saveFields(Save& save) { return {&save.playerName, &save.lobbyChoice}; }

// Chat during a match, shown by the game's HUD (point into the HUD view model; lines == nullptr = no chat in matches)
struct FlowChat {
    ChatLineVm* lines = nullptr;
    mealkit::u32 capacity = 0;
    mealkit::u32* count = nullptr;
    bool* open = nullptr;              // the input field is shown
    std::string* draft = nullptr;      // text input bound in the HUD (widget named chatInput)
    mealkit::ui::ScreenId screen = 0;  // the HUD screen (focus goes to its chatInput)
};
// For a HUD view model with MEALKIT_LIST(screens::ChatLineVm, chat, N), chatOpen and chatDraft
template <class Hud> FlowChat hudChat(Hud& hud, mealkit::ui::ScreenId screen) {
    return {hud.chat.data(), static_cast<mealkit::u32>(hud.chat.size()), &hud.chatCount, &hud.chatOpen, &hud.chatDraft, screen};
}

enum class Screen : mealkit::u8 { None, Title, Menu, Join, Lobby, Loading, Match, Message };

class Flow {
public:
    // Every frame (at the very start of the game's update)
    void update(mealkit::game::GameContext& ctx, const FlowGame& game, FlowSave save, FlowChat chat, mealkit::f64 dt);
    // The match ended (GameInstance::stop): close the pause/results screens
    void stop(mealkit::game::GameContext& ctx);

    Screen screen() const { return m_screen; }
    bool blocksInput() const;      // during menus/pause/chat/results: no input to the game and the camera stays
    bool pausesSimulation() const; // pause in solo play (with several players the match continues)
    bool takeSaveChanged();        // the name/choice changed (write the save)

    // Name of a player index (name set in the lobby, P1.. if none)
    static std::string playerName(const mealkit::session::Session& session, mealkit::u32 slot);

private:
    void show(mealkit::game::GameContext& ctx, Screen screen);
    void closeAll(mealkit::game::GameContext& ctx);
    void updateMenu(mealkit::game::GameContext& ctx, FlowSave save);
    void updateJoin(mealkit::game::GameContext& ctx);
    void updateLobby(mealkit::game::GameContext& ctx, const FlowGame& game, FlowSave save);
    void updateMatch(mealkit::game::GameContext& ctx, const FlowGame& game, FlowChat chat);
    void fillChat(const mealkit::session::Session& session, ChatLineVm* lines, mealkit::u32& count, mealkit::u32 capacity);
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
    mealkit::u32 m_choice = 0; // lobby choice (FlowSave::lobbyChoice when the game saves it)
};

} // namespace screens

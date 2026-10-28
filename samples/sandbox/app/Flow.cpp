#include "app/Flow.h"

#include "sim/Loadout.h"
#include "view/ViewTuning.h"

#include "mealkit/core/Version.h"
#include "mealkit/game/Loadout.h"
#include "mealkit/game/MatchService.h"
#include "mealkit/input/Actions.h"
#include "mealkit/loc/Strings.h"
#include "mealkit/platform/Window.h"
#include "mealkit/settings/Settings.h"

#include <algorithm>
#include <array>

using namespace mealkit;

namespace sandbox {
namespace {

// 화면 층 (위가 먼저 입력을 받는다). 설정 화면은 엔진이 더 위에 연다
constexpr i32 kMenuLayer = 10;
constexpr i32 kResultsLayer = 15;
constexpr i32 kPauseLayer = 20;

// 몸 종류 이름 (tables/bodies.table 행 순서, 로비의 선택 상자와 같은 키)
const char* bodyKey(u32 body) {
    static const char* keys[] = {"body.normal", "body.light", "body.heavy"};
    return body < 3 ? keys[body] : keys[0];
}

} // namespace

std::string Flow::playerName(const net::LockstepSession& session, u32 slot) {
    for (const net::LobbyMember& m : session.lobbyMembers())
        if (m.player == slot && !m.name.empty()) return m.name;
    return "P" + std::to_string(slot + 1);
}

bool Flow::blocksInput() const { return m_screen != Screen::Match || m_paused || m_chatOpen || m_ids.results != 0; }
bool Flow::pausesSimulation() const { return m_paused && m_pause.offline; }

bool Flow::takeSaveChanged() {
    const bool changed = m_saveChanged;
    m_saveChanged = false;
    return changed;
}

void Flow::closeAll(game::GameContext& ctx) {
    for (ui::ScreenId* id : {&m_ids.title, &m_ids.menu, &m_ids.join, &m_ids.lobby, &m_ids.loading, &m_ids.message,
                             &m_ids.pause, &m_ids.results}) {
        if (*id) ctx.ui.close(*id);
        *id = 0;
    }
    m_paused = m_chatOpen = m_chatFocusPending = false;
}

void Flow::show(game::GameContext& ctx, Screen screen) {
    closeAll(ctx);
    m_screen = screen;
    ctx.match.setBrowsing(screen == Screen::Join); // LAN 방 찾기는 참가 화면 동안만
    switch (screen) {
    case Screen::Title:
        m_ids.title = ctx.ui.open("ui/title.ui", kMenuLayer);
        ctx.ui.bind(m_ids.title, m_title);
        break;
    case Screen::Menu:
        m_ids.menu = ctx.ui.open("ui/menu.ui", kMenuLayer);
        ctx.ui.bind(m_ids.menu, m_menu);
        break;
    case Screen::Join:
        m_join.error.clear();
        m_ids.join = ctx.ui.open("ui/join.ui", kMenuLayer);
        ctx.ui.bind(m_ids.join, m_join);
        break;
    case Screen::Lobby:
        m_lobby.ready = ctx.session.localReady();
        m_lobby.chatDraft.clear();
        m_ids.lobby = ctx.ui.open("ui/lobby.ui", kMenuLayer);
        ctx.ui.bind(m_ids.lobby, m_lobby);
        break;
    case Screen::Loading: m_ids.loading = ctx.ui.open("ui/loading.ui", kMenuLayer); break;
    case Screen::Message:
        m_ids.message = ctx.ui.open("ui/message.ui", kMenuLayer);
        ctx.ui.bind(m_ids.message, m_message);
        break;
    case Screen::Match:
    case Screen::None: break; // HUD는 GameSession이 매치를 시작할 때 연다
    }
}

void Flow::stop(game::GameContext& ctx) {
    for (ui::ScreenId* id : {&m_ids.pause, &m_ids.results}) {
        if (*id) ctx.ui.close(*id);
        *id = 0;
    }
    m_paused = m_chatOpen = false;
}

void Flow::update(game::GameContext& ctx, const World* world, SaveData& save, HudVm& hud, ui::ScreenId hudScreen, f64 dt) {
    m_time += dt;
    net::LockstepSession& session = ctx.session;
    if (!m_nameLoaded) { // 처음: 세이브의 이름 (없으면 엔진이 정한 기본 이름: 계정·사용자 이름)
        m_nameLoaded = true;
        if (!save.playerName.empty()) session.setLocalName(save.playerName);
        m_menu.playerName = session.localName();
        const game::BuildInfo& build = ctx.match.config().build;
        m_version = build.gameVersion[0] ? std::string("v") + build.gameVersion + " (" + build.commit + ")" : std::string();
        m_title.version = (m_version.empty() ? std::string() : m_version + "  ·  ") + "MealKit " + MEALKIT_VERSION;
    }

    // 지금 보일 화면: 세션 단계가 정한다 (세션이 없으면 메뉴 쪽에서 고른 화면)
    Screen target = m_menuScreen;
    switch (ctx.match.phase()) {
    case game::MatchPhase::Menu: target = m_menuScreen; break;
    case game::MatchPhase::Opening:
    case game::MatchPhase::Connecting:
    case game::MatchPhase::Lobby: target = Screen::Lobby; break;
    case game::MatchPhase::Running: target = world ? Screen::Match : Screen::Loading; break;
    case game::MatchPhase::Ended: target = Screen::Message; break;
    }
    if (target != m_screen) show(ctx, target);

    switch (m_screen) {
    case Screen::Title: {
        bool next = ctx.actions.pressed("ui.confirm") || ctx.actions.pressed("jump");
        for (const ui::Action& a : ctx.ui.actions()) next = next || a.name == "title.start";
        if (next) {
            m_menuScreen = Screen::Menu;
            show(ctx, Screen::Menu);
        }
        break;
    }
    case Screen::Menu: updateMenu(ctx, save); break;
    case Screen::Join: updateJoin(ctx); break;
    case Screen::Lobby: updateLobby(ctx, save); break;
    case Screen::Match:
        if (world) updateMatch(ctx, *world, hud, hudScreen, dt);
        break;
    case Screen::Message:
        m_message.text = endText(ctx);
        m_message.title = ctx.strings.get(ctx.session.rejectInfo().reason != net::RejectReason::None ? "message.rejected" : "message.title");
        for (const ui::Action& a : ctx.ui.actions())
            if (a.name == "message.ok") {
                ctx.match.leave();
                m_menuScreen = Screen::Menu;
            }
        break;
    case Screen::Loading:
    case Screen::None: break;
    }
}

// 끊긴 이유: 접속 거부면 무엇이 다른지 (버전, 플랫폼, 게임 데이터), 아니면 세션의 이유
std::string Flow::endText(game::GameContext& ctx) const {
    const net::RejectInfo& r = ctx.session.rejectInfo();
    switch (r.reason) {
    case net::RejectReason::Version:
        return ctx.strings.format("reject.version", {{"mine", ctx.match.config().build.gameVersion}, {"host", r.hostVersion}});
    case net::RejectReason::Platform: return ctx.strings.format("reject.platform", {{"host", r.hostGroup}});
    case net::RejectReason::Content: {
        static const char* keys[] = {"content.scene", "content.models", "content.tuning", "content.project", "content.tables"};
        std::string parts;
        for (u32 i = 0; i < 5; ++i)
            if (r.contentMask & (1u << i)) parts += (parts.empty() ? "" : ", ") + ctx.strings.get(keys[i]);
        return ctx.strings.format("reject.content", {{"parts", parts}});
    }
    case net::RejectReason::Full: return ctx.strings.get("reject.full");
    case net::RejectReason::InProgress: return ctx.strings.get("reject.started");
    case net::RejectReason::None: break;
    }
    return ctx.match.error();
}

void Flow::updateMenu(game::GameContext& ctx, SaveData& save) {
    m_menu.onlineAvailable = ctx.match.onlineAvailable();
    // 이름: 다 친 뒤(글자 입력이 끝나면) 세션과 세이브에
    if (!ctx.ui.editingText() && m_menu.playerName != ctx.session.localName()) {
        ctx.session.setLocalName(m_menu.playerName);
        m_menu.playerName = ctx.session.localName(); // 길이 제한으로 잘린 것
        save.playerName = m_menu.playerName;
        m_saveChanged = true;
    }
    for (const ui::Action& a : ctx.ui.actions()) {
        bool ok = true;
        if (a.name == "menu.solo") ok = ctx.match.playOffline();
        else if (a.name == "menu.host") ok = ctx.match.host();
        else if (a.name == "menu.online") ok = ctx.match.hostOnline();
        else if (a.name == "menu.join") {
            m_menuScreen = Screen::Join;
            show(ctx, Screen::Join);
            return;
        } else if (a.name == "menu.quit") ctx.window.close();
        else if (a.name == "menu.back") {
            m_menuScreen = Screen::Title;
            show(ctx, Screen::Title);
            return;
        }
        m_menu.error = ok ? std::string() : ctx.match.error();
    }
}

void Flow::updateJoin(game::GameContext& ctx) {
    const std::vector<game::RoomEntry>& rooms = ctx.match.rooms();
    m_join.roomsCount = std::min<u32>(static_cast<u32>(rooms.size()), 12);
    for (u32 i = 0; i < m_join.roomsCount; ++i) {
        RoomRowVm& row = m_join.rooms[i];
        row.hostName = rooms[i].hostName;
        row.players = std::to_string(rooms[i].players) + "/" + std::to_string(rooms[i].maxPlayers);
        row.address = rooms[i].address;
        row.compatible = rooms[i].compatible;
        row.note = rooms[i].compatible ? std::string()
                   : rooms[i].sameBuild   ? ctx.strings.get("join.otherData")
                                          : ctx.strings.format("join.otherVersion", {{"version", rooms[i].version}});
    }
    m_join.noRooms = m_join.roomsCount == 0;
    for (const ui::Action& a : ctx.ui.actions()) {
        if (a.name == "join.back") {
            m_menuScreen = Screen::Menu;
            show(ctx, Screen::Menu);
            return;
        }
        std::string address;
        if (a.name == "join.room" && a.listIndex >= 0 && static_cast<u32>(a.listIndex) < m_join.roomsCount)
            address = m_join.rooms[a.listIndex].address;
        if (a.name == "join.address") address = m_join.address;
        if (address.empty()) continue;
        m_join.error = ctx.match.join(address) ? std::string() : ctx.match.error();
    }
}

void Flow::updateLobby(game::GameContext& ctx, SaveData& save) {
    net::LockstepSession& session = ctx.session;
    const ViewTuning& look = ctx.viewTuning<ViewTuning>();
    const game::MatchPhase phase = ctx.match.phase();
    m_lobby.connecting = phase != game::MatchPhase::Lobby;
    m_lobby.isHost = session.isHost() && !m_lobby.connecting;
    m_lobby.offline = session.isOffline();
    m_lobby.online = ctx.match.usesOnline();
    m_lobby.title = ctx.strings.get(m_lobby.offline ? "lobby.solo"
                                    : m_lobby.online ? "lobby.online"
                                    : m_lobby.isHost ? "lobby.hosting"
                                                     : "lobby.joined");
    if (m_lobby.connecting) m_lobby.title = ctx.strings.get("lobby.connecting");

    // 내 준비·몸 (몸은 세이브에 남아 다음 매치의 로드아웃이 된다: GameSession::lobby)
    session.setLocalReady(m_lobby.ready);
    if (m_lobby.body != save.body) {
        save.body = std::min<u32>(m_lobby.body, 2);
        m_saveChanged = true;
    }
    m_lobby.body = save.body;

    const std::vector<net::LobbyMember>& members = session.lobbyMembers();
    m_lobby.playersCount = std::min<u32>(static_cast<u32>(members.size()), net::kMaxPlayers);
    bool allReady = true;
    for (u32 i = 0; i < m_lobby.playersCount; ++i) {
        const net::LobbyMember& m = members[i];
        LobbyRowVm& row = m_lobby.players[i];
        row.name = m.name;
        row.ready = m.ready;
        row.host = m.host;
        row.you = m.player == session.localPlayer();
        row.color = look.playerColors[i % look.playerColors.size()];
        Loadout l;
        game::readLobbyLoadout(m, l);
        row.body = ctx.strings.get(bodyKey(l.body));
        row.ping = m.host || !ctx.settings.values().gameplay.showNetwork
                       ? std::string()
                       : ctx.strings.format("hud.ping", {{"ping", std::to_string(m.pingMs)}});
        if (!m.host && !m.ready) allReady = false;
    }
    m_lobby.canStart = m_lobby.isHost && allReady;
    m_lobby.waiting = !m_lobby.isHost && !m_lobby.connecting;
    m_lobby.needReady = m_lobby.isHost && !m_lobby.offline && !allReady;
    m_lobby.version = m_version;
    m_lobby.slowWarning.clear();
    if (const u32 slow = ctx.match.slowPlayers()) { // 입력 지연 상한(mealkit.project)보다 먼 사람
        std::string names;
        for (const net::LobbyMember& m : members)
            if (m.player < 32 && (slow & (1u << m.player))) names += (names.empty() ? "" : ", ") + m.name;
        m_lobby.slowWarning = ctx.strings.format("lobby.slow", {{"names", names}});
    }
    fillChat(session, m_lobby.chat.data(), m_lobby.chatCount, 8);

    for (const ui::Action& a : ctx.ui.actions()) {
        if (a.name == "lobby.start" && m_lobby.canStart) ctx.match.start();
        else if (a.name == "lobby.leave") {
            ctx.match.leave();
            m_menuScreen = Screen::Menu;
        } else if (a.name == "lobby.invite") ctx.match.inviteFriends();
        else if (a.name == "chat.send") {
            session.sendChat(m_lobby.chatDraft);
            m_lobby.chatDraft.clear();
        }
    }
}

void Flow::fillChat(const net::LockstepSession& session, ChatLineVm* lines, u32& count, u32 capacity) {
    const std::vector<net::ChatMessage>& log = session.chatLog();
    count = std::min<u32>(static_cast<u32>(log.size()), capacity);
    const size_t first = log.size() - count;
    for (u32 i = 0; i < count; ++i) {
        lines[i].name = log[first + i].name;
        lines[i].text = log[first + i].text;
    }
}

void Flow::fillResults(game::GameContext& ctx, const World& world) {
    const WorldState& s = world.state;
    const ViewTuning& look = ctx.viewTuning<ViewTuning>();
    std::array<u32, net::kMaxPlayers> order{};
    const u32 count = std::min<u32>(s.playerCount, net::kMaxPlayers);
    for (u32 i = 0; i < count; ++i) order[i] = i;
    // 적게 떨어진 순서 (같으면 플레이어 번호 순)
    std::stable_sort(order.begin(), order.begin() + count, [&](u32 a, u32 b) { return s.slots[a].falls < s.slots[b].falls; });
    m_results.rowsCount = count;
    for (u32 i = 0; i < count; ++i) {
        const u32 slot = order[i];
        ResultRowVm& row = m_results.rows[i];
        row.rank = i > 0 && s.slots[slot].falls == s.slots[order[i - 1]].falls ? m_results.rows[i - 1].rank : i + 1;
        row.name = playerName(ctx.session, slot);
        row.falls = s.slots[slot].falls;
        row.you = slot == ctx.session.localPlayer();
        row.winner = row.rank == 1;
        row.color = look.playerColors[slot];
    }
    m_results.isHost = ctx.session.isHost();
    const f64 left = std::max(0.0, static_cast<f64>(look.resultsSeconds) - (m_time - m_resultsTime));
    m_results.secondsLeft = static_cast<u32>(left + 0.999);
    if (m_results.isHost && left <= 0.0) ctx.match.returnToLobby(); // 방장: 잠시 뒤 모두 로비로
}

void Flow::updateMatch(game::GameContext& ctx, const World& world, HudVm& hud, ui::ScreenId hudScreen, f64 dt) {
    (void)dt;
    net::LockstepSession& session = ctx.session;
    const ViewTuning& look = ctx.viewTuning<ViewTuning>();

    // 결과: 시뮬레이션이 끝났다고 하면 (모두 같은 틱에)
    if (world.state.phase == MatchPhase::Finished && !m_ids.results) {
        if (m_ids.pause) ctx.ui.close(m_ids.pause);
        m_ids.pause = 0;
        m_paused = m_chatOpen = false;
        m_resultsTime = m_time;
        m_ids.results = ctx.ui.open("ui/results.ui", kResultsLayer);
        ctx.ui.bind(m_ids.results, m_results);
    }
    if (m_ids.results) fillResults(ctx, world);

    // 일시정지: Esc(menu.pause). 열려 있으면 화면의 뒤로(ui.back)가 닫는다
    const bool pauseWasOpen = m_paused;
    if (!pauseWasOpen && !m_ids.results && !m_chatOpen && !ctx.settings.isOpen() && ctx.actions.pressed("menu.pause")) {
        m_paused = true;
        m_pause.offline = session.isOffline();
        m_pause.isHost = session.isHost() && !session.isOffline();
        m_ids.pause = ctx.ui.open("ui/pause.ui", kPauseLayer);
        ctx.ui.bind(m_ids.pause, m_pause);
    }

    // 채팅 (여러 명): Enter로 HUD의 입력 칸을 열고, Enter로 보낸다. Esc면 그냥 닫힌다
    if (!session.isOffline() && !m_chatOpen && !m_paused && !m_ids.results && ctx.actions.pressed("chat.open")) {
        m_chatOpen = m_chatFocusPending = true; // 입력 칸이 보인 다음 프레임에 글자 치기를 시작한다
        hud.chatDraft.clear();
    } else if (m_chatOpen && m_chatFocusPending) {
        if (ctx.ui.focusText(hudScreen, "chatInput")) m_chatFocusPending = false;
    } else if (m_chatOpen && !ctx.ui.editingText()) {
        m_chatOpen = false;
    }
    const std::vector<net::ChatMessage>& log = session.chatLog();
    if (!log.empty() && log.back().serial != m_lastChatSerial) {
        m_lastChatSerial = log.back().serial;
        m_chatShownUntil = m_time + look.chatShowSeconds;
    }
    fillChat(session, hud.chat.data(), hud.chatCount, 5);
    if (!m_chatOpen && m_time > m_chatShownUntil) hud.chatCount = 0;
    hud.chatOpen = m_chatOpen;

    for (const ui::Action& a : ctx.ui.actions()) {
        if (a.name == "chat.send") {
            session.sendChat(hud.chatDraft);
            hud.chatDraft.clear();
            m_chatOpen = false;
        } else if (a.name == "pause.resume" && m_paused) {
            ctx.ui.close(m_ids.pause);
            m_ids.pause = 0;
            m_paused = false;
        } else if (a.name == "pause.lobby" || a.name == "results.lobby") {
            ctx.match.returnToLobby();
        } else if (a.name == "pause.leave" || a.name == "results.leave") {
            ctx.match.leave();
            m_menuScreen = Screen::Menu;
        }
    }
}

} // namespace sandbox

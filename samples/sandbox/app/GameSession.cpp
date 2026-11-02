#include "app/GameSession.h"
#include "app/Headless.h"
#include "sim/Loadout.h"
#include "sim/Commands.h"

#include "mealkit/feedback/Feedback.h"
#include "mealkit/game/GameData.h"
#include "mealkit/game/Loadout.h"
#include "mealkit/game/Save.h"
#include "mealkit/loc/Strings.h"
#include "mealkit/platform/Window.h"
#include "mealkit/settings/Settings.h"
#include "mealkit/sim/Commands.h"
#include "mealkit/ui/Ui.h"

#if MEALKIT_DEBUG_UI
#include <imgui.h>
#endif

#include <algorithm>
#include <cstring>

using namespace mealkit;

namespace sandbox {
namespace {

// Lobby body choice: string keys in tables/bodies.table row order (the same keys as the select box in ui/lobby.ui)
constexpr const char* kBodyKeys[] = {"body.normal", "body.light", "body.heavy"};

u32 memberBody(const net::LobbyMember& member) {
    Loadout l;
    game::readLobbyLoadout(member, l);
    return l.body;
}

} // namespace

void GameSession::loadSave(const std::string& userFolder) {
    if (m_saveLoaded) return;
    game::readSave(m_save, userFolder, "progress", kSaveVersion);
    m_saveLoaded = true;
}

// Before the match: loadout (bots get a different body per player, humans the body chosen in the lobby = save). The lobby screen picks the body (app/Flow.cpp)
void GameSession::lobby(net::LockstepSession& session, const game::PlayOptions& options, const std::string& userFolder) {
    if (options.bot) {
        Headless::botLoadout(session.localPlayer(), session);
        return;
    }
    loadSave(userFolder);
    Loadout l;
    l.body = m_save.lobbyChoice;
    game::setLoadout(session, l);
}

void GameSession::start(game::GameContext& ctx, const scene::Scene& scene, const game::PlayOptions& options) {
    const net::LockstepSession& session = ctx.session;
    m_scene = scene::flattened(scene); // flattens the parent/child hierarchy to world coordinates (simulation and drawing only see the flat list)
    m_sceneSource = &scene;
    m_options = options;
    Tuning tuning;
    game::loadTuning(reflect::typeOf<Tuning>(), &tuning, options.tuningPath);
    const game::MatchInfo match = game::matchInfo(session); // seed, player count, match data, per-player loadouts
    m_sim.start(options, game::tickDt(), [&](World& world, u32 physicsThreads) {
        Headless::startWorld(world, match, tuning, m_scene, ctx.models, physicsThreads); // same match start as headless
    });
    m_camera.load(m_scene, ctx.viewTuning<ViewTuning>().camera); // the scene's virtual cameras, assets/tables/cutscenes.table
    m_lastPhase = MatchPhase::Waiting;
    m_input.clearLatches();
    m_prediction.reset();
    m_effects = {};
    m_view.anims.clear();
    m_view.animTime = -1.0;
    m_view.animStates.load(); // assets/tables/animation.table (character animation states)
    m_hudVm = {};
    // Music (assets/sounds.events "music arena"): bass alone until the round starts, the tension parameter brings in drums and arpeggio
    ctx.audio.setGlobalParam("tension", 0.0f);
    ctx.audio.playMusic("arena");
    loadSave(ctx.userFolder);
    ++m_save.matchesStarted;
    game::writeSave(m_save, ctx.userFolder, "progress", kSaveVersion);
    if (!ctx.ui.isOpen(m_hud)) m_hud = ctx.ui.open("ui/hud.ui");
    if (!ctx.ui.isOpen(m_nameTag)) m_nameTag = ctx.ui.openWorld("ui/nametag.ui");
    ctx.ui.bind(m_hud, m_hudVm);
}

// Match end (back to lobby, left, disconnected): discard the World and close match screens. The save remains
void GameSession::stop(game::GameContext& ctx) {
    m_sim.stop();
    ctx.audio.stopMusic(1.0f);
    ctx.audio.setSnapshot("pause", false);
    if (m_hud) ctx.ui.close(m_hud);
    if (m_nameTag) ctx.ui.close(m_nameTag);
    m_hud = m_nameTag = 0;
    m_flow.stop(ctx);
    m_frozen = false;
    game::writeSave(m_save, ctx.userFolder, "progress", kSaveVersion);
}

void GameSession::setTuning(const void* tuning) {
    const Tuning& value = *static_cast<const Tuning*>(tuning);
    m_sim.modifyWorlds([&](World& world) { world.setTuning(value); });
}

void GameSession::update(game::GameContext& ctx, f64 dt) {
    net::LockstepSession& session = ctx.session;
    const ViewTuning& look = ctx.viewTuning<ViewTuning>();
    loadSave(ctx.userFolder);
    // Screen flow: opens screens matching the session phase and handles buttons (menu/lobby/pause/results/chat)
    FlowGame flow;
    flow.playerColors = look.playerColors;
    flow.resultsSeconds = look.resultsSeconds;
    flow.chatShowSeconds = look.chatShowSeconds;
    flow.choices = kBodyKeys;
    flow.memberChoice = memberBody;
    if (m_sim.started()) {
        const WorldState& s = m_sim.world().state;
        flow.inMatch = true;
        flow.results = s.flow.phase == MatchPhase::Results;
        flow.playerCount = s.playerCount;
        for (u32 i = 0; i < s.playerCount; ++i) flow.score[i] = s.slots[i].falls;
        flow.lowestFirst = true; // fewest falls wins
    }
    m_flow.update(ctx, flow, m_save, m_hudVm, m_hud, dt);
    if (m_flow.takeSaveChanged()) game::writeSave(m_save, ctx.userFolder, "progress", kSaveVersion);
    if (!m_sim.started()) {
        if (ctx.scene && ctx.scene != m_sceneSource) { // arena shown behind the menu
            m_scene = scene::flattened(*ctx.scene);
            m_sceneSource = ctx.scene;
        }
        return;
    }

    const bool menuOpen = m_flow.blocksInput(); // during pause/chat/results: the character stands still and the camera stays
    if (!menuOpen && !m_camera.brain.blocksInput()) {
        m_camera.control(ctx.window, ctx.actions, look.camera);
        m_input.poll(ctx.actions);
        // Debug cheats don't modify state directly; they're sent as confirmed commands (all peers process them on the same tick)
        if (ctx.actions.pressed("debug.shockwave")) sim::sendCommand(session, DebugShockwave{});
        if (ctx.actions.pressed("menu.settings")) ctx.settings.open(); // standard settings screen (the HUD's [Settings] button calls settings.open directly from the .ui)
    } else
        m_camera.release(ctx.window);
    const bool hitStop = ctx.feedback.hitStop(); // freezes only the view: view clock/effects/camera focus (the simulation continues)
    if (!hitStop) m_viewTime += dt;
    m_view.animStates.poll(dt); // edits to animation.table apply while playing
    m_camera.cutscenes.poll(dt);
    m_effects.prune(m_viewTime, look.shockwave);

    // Every confirmed tick: build and send local input, and move tick events into view effects
    auto sample = [&](u32 tick) {
        GameInput gi = m_input.sample(ctx.actions, m_camera);
        if (m_flow.blocksInput() || m_camera.brain.blocksInput()) gi = GameInput{}; // cutscenes block only input, never the simulation
        m_input.clearLatches();
        if (m_options.bot) {
            Headless::botCommands(tick, session.localPlayer(), session);
            return Headless::botInput(tick, session.localPlayer());
        }
        net::PlayerInput pi;
        std::memcpy(pi.bytes, &gi, sizeof(gi));
        return pi;
    };
    auto onTick = [&](const World& world) {
        bool saveNow = false;
        for (const GameEvent& e : world.events) {
            if (e.player == session.localPlayer() && e.kind == EventKind::Fall) ++m_save.falls, saveNow = true;
            if (e.player == session.localPlayer() && e.kind == EventKind::Push) ++m_save.pushes;
            m_effects.onEvent(e, m_viewTime, look.shockwave, world.tuning.pawnHalfHeight);
            onWorldEvent(e, world.state, m_viewTime, m_view); // flash on a pushed character
            ctx.feedback.trigger(e, session.localPlayer()); // sound/shake/flash/hitstop are decided by the feedback table
        }
        if (saveNow) game::writeSave(m_save, ctx.userFolder, "progress", kSaveVersion);
        // Staging from simulation phases: every peer sees the same tick, so shared cutscenes start together
        const MatchPhase phase = world.state.flow.phase;
        if (phase != m_lastPhase) {
            if (phase == MatchPhase::Countdown && !look.camera.introCutscene.empty())
                m_camera.brain.playCutscene(m_camera.cutscenes, look.camera.introCutscene);
            if (phase == MatchPhase::Playing) m_camera.brain.stopCutscene(); // the countdown can be shorter than the clip
            m_camera.brain.setActive(look.camera.resultsCamera, phase == MatchPhase::Results); // game code only switches cameras on/off
            // Music follows the phase: full while playing, half at round end/results (with a stinger on the next bar, which ducks the music)
            ctx.audio.setGlobalParam("tension", phase == MatchPhase::Playing ? 1.0f : phase >= MatchPhase::RoundEnd ? 0.5f : 0.0f);
            if (phase == MatchPhase::RoundEnd) ctx.audio.stinger("stinger", audio::Quantize::Bar);
            m_lastPhase = phase;
        }
    };
    if (!m_flow.pausesSimulation()) m_sim.update(session, dt, sample, onTick); // pause in solo play halts the match
    ctx.audio.setSnapshot("pause", m_flow.blocksInput()); // menus over the match: music and effects muffled (snapshot "pause")

    // Moving my character ahead: by my movement inputs sent but not yet executed (view only, only during play)
    const World& world = m_sim.world();
    const u8 me = session.localPlayer();
    m_prediction.begin();
    m_sim.forEachPendingInput([&](u32, const net::PlayerInput& in) { m_prediction.addInput(world, me, in); });
    m_prediction.update(static_cast<f32>(dt), look.prediction, world.state.flow.phase == MatchPhase::Playing && playerPawn(world.state, me));

    // Game screen UI: state → view model, button actions (changes to the simulation go through confirmed commands)
    std::array<std::string, net::kMaxPlayers> names;
    for (u32 i = 0; i < names.size(); ++i) names[i] = Flow::playerName(session, i);
    fillHud(m_sim.world().state, m_sim.world().tuning, session.localPlayer(), look, names, m_hudVm);
    m_save.playSeconds += static_cast<f32>(dt);
    m_saveTimer += dt;
    if (m_saveTimer >= 30.0) {
        m_saveTimer = 0.0;
        game::writeSave(m_save, ctx.userFolder, "progress", kSaveVersion);
    }
    // Network display for players (turned off in player settings): per-player ping, and who it is when stalled waiting for input
    const bool showNet = ctx.settings.values().gameplay.showNetwork && !session.isOffline();
    for (u32 i = 0; i < m_hudVm.playersCount; ++i) {
        PlayerRowVm& row = m_hudVm.players[i];
        row.ping = session.pingMs(i);
        row.pingBad = showNet && i != 0 && row.ping >= look.badPingMs;
        row.pingGood = showNet && i != 0 && !row.pingBad;
    }
    m_hudVm.waiting.clear();
    if (const u32 mask = showNet ? session.waitingFor() : 0u) {
        std::string list;
        for (u32 i = 0; i < m_hudVm.playersCount; ++i)
            if (mask & (1u << i)) list += (list.empty() ? "" : ", ") + names[i];
        m_hudVm.waiting = ctx.strings.format("hud.waiting", {{"names", list}});
    }
    m_hudVm.totalFalls = m_save.falls;
    m_hudVm.playMinutes = static_cast<u32>(m_save.playSeconds / 60.0f);
    for (const ui::Action& a : ctx.ui.actions())
        if (a.name == "debug.shockwave") sim::sendCommand(session, DebugShockwave{});
}

void GameSession::buildFrame(game::GameContext& ctx, f64 now, render::FrameParams& frame, render::DrawList& out) {
    const u8 localPlayer = ctx.session.localPlayer();
    const bool started = m_sim.started();
    const ViewTuning& look = ctx.viewTuning<ViewTuning>();
    if (!started) { // behind the menu/lobby: slowly circles and shows the arena
        camera::Brain& menu = m_menuCamera.brain;
        menu.setFallback(look.camera.menu);
        camera::BrainInput in;
        in.dt = static_cast<f32>(std::clamp(now - m_menuTime, 0.0, 0.1));
        in.defaultFov = ctx.settings.values().display.fov;
        m_menuTime = now;
        menu.update(in);
        menu.apply(ctx.viewport, look.camera.nearPlane, look.camera.farPlane, frame);
        applyEnvironment(m_scene.environment, ctx.models, frame);
        applyProbes(m_scene, ctx.models, m_probes, frame);
        applyLights(m_scene, m_view, m_viewTime, m_lights, frame);
        drawScene(m_scene, ctx.models, frame.cameraPos, m_viewTime, out);
        return;
    }
    // When hitstop starts, hold the image of that moment, and return to the current state when it ends
    const bool hitStop = ctx.feedback.hitStop();
    if (hitStop && !m_frozen) {
        m_frozenPrevious = m_sim.previous();
        m_frozenCurrent = m_sim.world().state;
        m_frozenAlpha = m_sim.alpha();
    }
    m_frozen = hitStop;
    const WorldState& previous = m_frozen ? m_frozenPrevious : m_sim.previous();
    const WorldState& current = m_frozen ? m_frozenCurrent : m_sim.world().state;
    const f32 alpha = m_frozen ? m_frozenAlpha : m_sim.alpha();

    // The camera follows the Pawn I possess. While waiting to respawn it stays at the last spot (spectating).
    const Pawn* mine = nullptr;
    Vec3 target;
    if (localPlayer < current.playerCount) {
        const Controller* c = current.controllers.get(current.slots[localPlayer].controller);
        mine = c ? current.pawns.get(c->pawn) : nullptr;
        if (mine) {
            const Pawn* before = previous.pawns.get(c->pawn);
            target = lerp(before ? before->position : mine->position, mine->position, alpha) + m_prediction.offset;
        }
    }
    const f32 dt = static_cast<f32>(std::clamp(now - m_cameraTime, 0.0, 0.1)); // first frame / stalls: no jumps
    m_cameraTime = now;
    m_cameraEvents.clear();
    const Vec3 shake = ctx.feedback.cameraShake(mine ? target : m_camera.brain.pose().position); // impulses come from the feedback table
    m_camera.update(m_sim.world(), mine, mine ? &target : nullptr, hitStop ? 0.0f : dt, ctx.settings.values().display.fov, shake,
                    m_cameraEvents);
    m_camera.apply(ctx.viewport, look.camera, frame);
    for (const std::string& row : m_cameraEvents) ctx.feedback.play(row, frame.cameraPos); // cutscene events
    applyEnvironment(m_scene.environment, ctx.models, frame);
    applyProbes(m_scene, ctx.models, m_probes, frame);
    applyLights(m_scene, m_view, m_viewTime, m_lights, frame);

    drawScene(m_scene, ctx.models, frame.cameraPos, m_viewTime, out);
    m_view.animEvents.clear();
    drawWorld(previous, current, alpha, localPlayer, now, m_viewTime, look, ctx.models, m_view, out, m_prediction.offset);
    for (const WorldView::AnimEvent& e : m_view.animEvents) ctx.feedback.play(e.row, e.position); // footsteps etc. (feedback table rows)
    // Name tags above characters (world-space UI): above the interpolated position
    for (u32 slot = 0; slot < current.playerCount && slot < m_tagVms.size(); ++slot) {
        const Pawn* pawn = playerPawn(current, slot);
        if (!pawn) continue;
        const Controller* c = current.controllers.get(current.slots[slot].controller);
        const Pawn* before = c ? previous.pawns.get(c->pawn) : nullptr;
        const Vec3 at = lerp(before ? before->position : pawn->position, pawn->position, alpha) +
                        (slot == localPlayer ? m_prediction.offset : Vec3{});
        NameTagVm& tag = m_tagVms[slot];
        tag.name = Flow::playerName(ctx.session, slot);
        tag.you = slot == localPlayer;
        tag.color = look.playerColors[slot];
        ctx.ui.drawAt(m_nameTag, at + Vec3{0.0f, look.nameTagHeight, 0.0f}, tag);
    }
    m_effects.draw(m_viewTime, look.shockwave, ctx.models.boxMesh(), out);
}

// Debug window (F1 in dev builds, during editor play). Buttons that change the simulation send confirmed commands.
void GameSession::debugUi(game::GameContext& ctx) {
#if MEALKIT_DEBUG_UI
    if (!m_sim.started()) return;
    const World& world = m_sim.world();
    const WorldState& state = world.state;
    ImGui::SetNextWindowPos(ImVec2(ctx.viewport.x + 10.0f, ctx.viewport.y + 10.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Sandbox", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("tick %u | player %u of %u | phase %u", state.tick, ctx.session.localPlayer() + 1, state.playerCount,
                    static_cast<u32>(state.flow.phase));
        ImGui::Text("pawns %u | boxes %u | moving bodies %u", state.pawns.count(), state.props.count(), world.activeBodies());
        for (u32 i = 0; i < state.playerCount; ++i) ImGui::Text("P%u falls %u", i + 1, state.slots[i].falls);
        if (ImGui::Button("Shockwave (F8)")) sim::sendCommand(ctx.session, DebugShockwave{});
        ImGui::SameLine();
        ImGui::TextDisabled("sent as a command");
    }
    ImGui::End();
#else
    (void)ctx;
#endif
}

} // namespace sandbox

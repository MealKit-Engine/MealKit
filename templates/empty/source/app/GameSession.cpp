#include "app/GameSession.h"
#include "app/Headless.h"

#include "mealkit/feedback/Feedback.h"
#include "mealkit/game/GameData.h"
#include "mealkit/game/Save.h"
#include "mealkit/platform/Window.h"
#include "mealkit/settings/Settings.h"
#include "mealkit/ui/Ui.h"

#include <algorithm>
#include <cstring>

using namespace mealkit;

namespace sandbox {

void GameSession::loadSave(const std::string& userFolder) {
    if (m_saveLoaded) return;
    game::readSave(m_save, userFolder, "progress", kSaveVersion);
    m_saveLoaded = true;
}

// Before the match: match start data (mealkit/game/Loadout.h) goes here, e.g. game::setLoadout(session, loadout) from a lobby choice
void GameSession::lobby(net::LockstepSession& session, const game::PlayOptions& options, const std::string& userFolder) {
    (void)session;
    (void)options;
    loadSave(userFolder);
}

void GameSession::start(game::GameContext& ctx, const scene::Scene& scene, const game::PlayOptions& options) {
    m_scene = scene::flattened(scene); // flattens the parent/child hierarchy to world coordinates
    m_sceneSource = &scene;
    m_options = options;
    Tuning tuning;
    game::loadTuning(reflect::typeOf<Tuning>(), &tuning, options.tuningPath);
    const game::MatchInfo match = game::matchInfo(ctx.session); // seed, player count, match data, per-player loadouts
    sim().start(options, game::tickDt(), [&](World& world, u32 physicsThreads) {
        Headless::startWorld(world, match, tuning, m_scene, ctx.models, physicsThreads); // same match start as headless
    });
    m_camera.load(m_scene, ctx.viewTuning<ViewTuning>().camera);
    m_input.clearLatches();
    m_hudVm = {};
    loadSave(ctx.userFolder);
    ++m_save.matchesStarted;
    game::writeSave(m_save, ctx.userFolder, "progress", kSaveVersion);
    if (!ctx.ui.isOpen(m_hud)) m_hud = ctx.ui.open("ui/hud.ui");
    ctx.ui.bind(m_hud, m_hudVm);
}

// Match end (back to lobby, left, disconnected): discard the World and close match screens
void GameSession::stop(game::GameContext& ctx) {
    sim().stop();
    if (m_hud) ctx.ui.close(m_hud);
    m_hud = 0;
    m_flow.stop(ctx);
    game::writeSave(m_save, ctx.userFolder, "progress", kSaveVersion);
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
    if (sim().started()) {
        const WorldState& s = sim().world().state;
        flow.inMatch = true;
        flow.results = s.flow.phase == MatchPhase::Results;
        flow.playerCount = s.playerCount;
        for (u32 i = 0; i < s.playerCount; ++i) flow.score[i] = s.slots[i].score;
    }
    m_flow.update(ctx, flow, m_save, m_hudVm, m_hud, dt);
    if (m_flow.takeSaveChanged()) game::writeSave(m_save, ctx.userFolder, "progress", kSaveVersion);
    if (!sim().started()) {
        if (ctx.scene && ctx.scene != m_sceneSource) { // the scene shown behind the menu
            m_scene = scene::flattened(*ctx.scene);
            m_sceneSource = ctx.scene;
        }
        return;
    }

    if (!m_flow.blocksInput()) { // during pause/chat/results: the character stands still and the camera stays
        m_camera.control(ctx.window, ctx.actions, look.camera);
        m_input.poll(ctx.actions);
        if (ctx.actions.pressed("menu.settings")) ctx.settings.open(); // standard settings screen
    } else
        m_camera.release(ctx.window);
    m_viewTime += dt;

    // Every confirmed tick: build and send local input, and pass tick events to the feedback table (sound, shake, flash)
    auto sample = [&](u32 tick) {
        GameInput gi = m_input.sample(ctx.actions, m_camera);
        if (m_flow.blocksInput()) gi = GameInput{};
        m_input.clearLatches();
        if (m_options.bot) return Headless::botInput(tick, session.localPlayer());
        net::PlayerInput pi;
        std::memcpy(pi.bytes, &gi, sizeof(gi));
        return pi;
    };
    auto onTick = [&](const World& world) {
        for (const GameEvent& e : world.events) ctx.feedback.trigger(e, session.localPlayer());
    };
    if (!m_flow.pausesSimulation()) sim().update(session, dt, sample, onTick); // pause in solo play halts the match
    fillHud(sim().world().state, sim().world().tuning, m_hudVm);
}

void GameSession::buildFrame(game::GameContext& ctx, f64 now, render::FrameParams& frame, render::DrawList& out) {
    const ViewTuning& look = ctx.viewTuning<ViewTuning>();
    if (!sim().started()) { // behind the menu/lobby: slowly circles the scene
        camera::Brain& menu = m_menuCamera.brain;
        menu.setFallback(look.camera.menu);
        camera::BrainInput in;
        in.dt = static_cast<f32>(std::clamp(now - m_menuTime, 0.0, 0.1));
        in.defaultFov = ctx.settings.values().display.fov;
        m_menuTime = now;
        menu.update(in);
        menu.apply(ctx.viewport, look.camera.nearPlane, look.camera.farPlane, frame);
    } else {
        // The camera follows my character. While waiting to respawn it stays at the last spot
        const WorldState& previous = sim().previous();
        const WorldState& current = sim().world().state;
        const Pawn* mine = playerPawn(current, ctx.session.localPlayer());
        Vec3 target;
        if (mine) {
            const Pawn* before = playerPawn(previous, ctx.session.localPlayer());
            target = lerp(before ? before->position : mine->position, mine->position, sim().alpha());
        }
        const f32 dt = static_cast<f32>(std::clamp(now - m_cameraTime, 0.0, 0.1)); // first frame / stalls: no jumps
        m_cameraTime = now;
        m_camera.update(sim().world(), mine, mine ? &target : nullptr, dt, ctx.settings.values().display.fov);
        m_camera.apply(ctx.viewport, look.camera, frame);
    }
    applyEnvironment(m_scene.environment, ctx.models, frame);
    applyProbes(m_scene, ctx.models, m_probes, frame);
    applyLights(m_scene, m_lights, frame);
    drawScene(m_scene, ctx.models, frame.cameraPos, m_viewTime, out);
    if (sim().started()) drawWorld(sim().previous(), sim().world().state, sim().alpha(), look, ctx.models, out);
}

} // namespace sandbox

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
void GameSession::lobby(session::Session& session, const game::PlayOptions& options, const std::string& userFolder) {
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
    game::writeSave(m_save, ctx.userFolder, "progress", kSaveVersion);
}

void GameSession::update(game::GameContext& ctx, f64 dt) {
    session::Session& session = ctx.session;
    loadSave(ctx.userFolder);
    if (!sim().started()) {
        if (ctx.scene && ctx.scene != m_sceneSource) { // the scene shown behind the menu
            m_scene = scene::flattened(*ctx.scene);
            m_sceneSource = ctx.scene;
        }
        return;
    }

    m_input.poll(ctx.actions);
    if (ctx.actions.pressed("menu.settings")) ctx.settings.open(); // standard settings screen
    m_viewTime += dt;

    // Every confirmed tick: build and send local input, and pass tick events to the feedback table (sound, shake, flash)
    auto sample = [&](u32 tick) {
        GameInput gi = m_input.sample(ctx.actions, m_camera);
        m_input.clearLatches();
        if (m_options.bot) return Headless::botInput(tick, session.localPlayer());
        session::PlayerInput pi;
        std::memcpy(pi.bytes, &gi, sizeof(gi));
        return pi;
    };
    auto onTick = [&](const World& world) {
        for (const GameEvent& e : world.events) ctx.feedback.trigger(e, session.localPlayer());
    };
    sim().update(session, dt, sample, onTick);
    fillHud(sim().world().state, m_hudVm);
}

void GameSession::buildFrame(game::GameContext& ctx, f64 now, render::FrameParams& frame, render::DrawList& out) {
    const ViewTuning& look = ctx.viewTuning<ViewTuning>();
    if (!sim().started()) { // before the match starts: slowly circles the scene
        camera::Brain& menu = m_menuCamera;
        menu.setFallback(look.camera.menu);
        camera::BrainInput in;
        in.dt = static_cast<f32>(std::clamp(now - m_menuTime, 0.0, 0.1));
        in.defaultFov = ctx.settings.values().display.fov;
        m_menuTime = now;
        menu.update(in);
        menu.apply(ctx.viewport, 0.1f, 1000.0f, frame); // perspective orbit behind the menu (the 2D planes are orthographic)
    } else {
        m_camera.apply(ctx.viewport, look.camera, frame); // fixed side view (camrig module: Side2DCamera follows)
    }
    applyEnvironment(m_scene.environment, ctx.models, frame);
    applyProbes(m_scene, ctx.models, m_probes, frame);
    applyLights(m_scene, m_lights, frame);
    if (!m_sprites) m_sprites = std::make_unique<render::SpriteTextures>(ctx.renderer);
    drawScene(m_scene, ctx.models, *m_sprites, frame.cameraPos, m_viewTime, out);
    if (sim().started()) drawWorld(sim().previous(), sim().world().state, sim().alpha(), look, ctx.models, out);
}

} // namespace sandbox

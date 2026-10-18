#include "app/GameSession.h"
#include "app/Headless.h"

#include "mealkit/game/GameData.h"
#include "mealkit/platform/Window.h"

#include <cstring>

using namespace mealkit;

namespace sandbox {

void GameSession::start(game::GameContext& ctx, const scene::Scene& scene, const game::PlayOptions& options) {
    const net::LockstepSession& session = ctx.session;
    m_scene = scene::flattened(scene); // 부모·자식 계층을 월드 좌표로 펼친다 (시뮬레이션과 그리기는 평평한 목록만 본다)
    m_options = options;
    Tuning tuning;
    game::loadTuning(reflect::typeOf<Tuning>(), &tuning, options.tuningPath);
    const game::MatchInfo match{session.seed(), session.playerCount(), session.matchData()};
    m_sim.start(options, kTickDt, [&](World& world, u32 physicsThreads) {
        Headless::startWorld(world, match, tuning, m_scene, ctx.models, physicsThreads); // 헤드리스와 같은 매치 시작
    });
    m_input.clearLatches();
    m_effects = {};
}

void GameSession::setTuning(const void* tuning) {
    const Tuning& value = *static_cast<const Tuning*>(tuning);
    m_sim.modifyWorlds([&](World& world) { world.setTuning(value); });
}

void GameSession::update(game::GameContext& ctx, f64 dt) {
    net::LockstepSession& session = ctx.session;
    Input& input = ctx.window.input();
    const bool keyboardAllowed = ctx.keyboardAllowed, mouseAllowed = ctx.mouseAllowed;
    const ViewTuning& look = ctx.viewTuning<ViewTuning>();
    m_camera.control(ctx.window, mouseAllowed, look.camera);
    m_input.poll(input, keyboardAllowed, mouseAllowed);
    m_viewTime += dt;
    m_effects.prune(m_viewTime, look.shockwave);

    // 확정된 틱마다: 로컬 입력을 만들어 보내고, 틱 이벤트를 화면 효과로 옮긴다
    auto sample = [&](u32 tick) {
        const GameInput gi = m_input.sample(input, m_camera, keyboardAllowed, mouseAllowed);
        m_input.clearLatches();
        if (m_options.bot) return Headless::botInput(tick, session.localPlayer());
        net::PlayerInput pi;
        std::memcpy(pi.bytes, &gi, sizeof(gi));
        return pi;
    };
    auto onTick = [&](const World& world) {
        for (const GameEvent& e : world.events) m_effects.onEvent(e, m_viewTime, look.shockwave);
    };
    m_sim.update(session, dt, sample, onTick);
}

void GameSession::buildFrame(game::GameContext& ctx, f64 now, render::FrameParams& frame, render::DrawList& out) {
    const u8 localPlayer = ctx.session.localPlayer();
    const bool started = m_sim.started();
    const f32 alpha = m_sim.alpha();
    const ViewTuning& look = ctx.viewTuning<ViewTuning>();

    // 카메라는 내가 빙의한 Pawn을 따라간다. 리스폰을 기다리는 동안은 마지막 자리에 머문다 (관전).
    if (started && localPlayer < m_sim.world().state.playerCount) {
        const WorldState& current = m_sim.world().state;
        const Controller* c = current.controllers.get(current.slots[localPlayer].controller);
        const Pawn* pawn = c ? current.pawns.get(c->pawn) : nullptr;
        if (pawn) {
            const Pawn* before = m_sim.previous().pawns.get(c->pawn);
            m_focus = lerp(before ? before->position : pawn->position, pawn->position, alpha) +
                      Vec3{0, look.camera.focusHeight, 0};
        }
    }
    m_camera.apply(m_focus, ctx.viewport, look.camera, frame);
    applyEnvironment(m_scene.environment, frame);

    drawScene(m_scene, ctx.models, frame.cameraPos, out);
    if (!started) return;
    drawWorld(m_sim.previous(), m_sim.world().state, alpha, localPlayer, now, look, ctx.models.boxMesh(), m_view, out);
    m_effects.draw(m_viewTime, look.shockwave, ctx.models.boxMesh(), out);
}

} // namespace sandbox

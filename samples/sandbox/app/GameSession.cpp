#include "app/GameSession.h"

#include "mealkit/core/Clock.h"
#include "mealkit/core/Profile.h"
#include "mealkit/platform/Window.h"

#include <algorithm>
#include <cstring>

using namespace mealkit;

namespace sandbox {

void GameSession::start(const scene::Scene& scene, const net::LockstepSession& session, const game::PlayOptions& options,
                        asset::ModelCache& models) {
    m_scene = scene;
    m_options = options;
    m_models = &models;
    m_world.reset(session.seed(), session.playerCount(), session.matchData(), m_scene, models, options.physicsThreads);
    m_previous = m_world.state;
    m_started = true;
    m_finished = false;
    m_captureDone = m_captureDue = false;
    m_nextTick = 0;
    m_accumulator = 0.0;
    m_input.clearLatches();
}

bool GameSession::captureDue() {
    const bool due = m_captureDue;
    m_captureDue = false;
    return due;
}

f64 GameSession::takeSimMsPerTick() {
    const f64 ms = m_simTicks ? m_simSeconds / m_simTicks * 1000.0 : 0.0;
    m_simSeconds = 0.0;
    m_simTicks = 0;
    return ms;
}

void GameSession::update(f64 dt, net::LockstepSession& session, Window& window, bool keyboardAllowed, bool mouseAllowed) {
    Input& input = window.input();
    m_camera.control(window, mouseAllowed);
    m_input.poll(input, keyboardAllowed, mouseAllowed);

    if (!m_started || m_finished) return;
    MEALKIT_ZONE("Simulation");

    // 고정 30Hz, 확정된 틱만 실행
    m_accumulator += dt;
    int budget = 8; // 한 프레임에 따라잡을 최대 틱
    const f64 simStart = nowSeconds();
    while (m_accumulator >= kTickDt && budget-- > 0 && session.canExecute(m_nextTick)) {
        m_previous = m_world.state;
        m_world.step(session.inputs(m_nextTick));
        session.tickExecuted(m_nextTick, m_world.hash());

        const GameInput gi = m_options.bot ? botInput(m_nextTick, session.localPlayer())
                                           : m_input.sample(input, m_camera, keyboardAllowed, mouseAllowed);
        m_input.clearLatches();
        net::PlayerInput pi;
        std::memcpy(pi.bytes, &gi, sizeof(gi));
        session.submitLocalInput(m_nextTick + session.inputDelay(), pi);

        if (m_options.capture && !m_captureDone && m_nextTick >= m_options.captureTick) {
            m_captureDue = m_captureDone = true;
        }
        const bool last = m_options.exitTick && m_nextTick >= m_options.exitTick;
        ++m_nextTick;
        ++m_simTicks;
        m_accumulator -= kTickDt;
        if (last) {
            m_finished = true;
            break;
        }
    }
    m_simSeconds += nowSeconds() - simStart;
    // 입력을 기다리며 멈춘 동안 시간 빚이 무한히 쌓이지 않게 한다
    m_accumulator = std::min(m_accumulator, static_cast<f64>(kTickDt) * 8.0);
}

void GameSession::buildFrame(f64 now, u8 localPlayer, render::Viewport viewport, render::FrameParams& frame,
                             render::DrawList& out) {
    if (!m_models) return;
    const f32 alpha = m_started ? std::clamp(static_cast<f32>(m_accumulator / kTickDt), 0.0f, 1.0f) : 1.0f;
    const WorldState& current = m_world.state;

    Vec3 focus{0.0f, 1.0f, 0.0f};
    if (m_started && localPlayer < current.playerCount)
        focus = lerp(m_previous.players[localPlayer].position, current.players[localPlayer].position, alpha) + Vec3{0, 0.8f, 0};
    m_camera.apply(focus, viewport, frame);
    applyEnvironment(m_scene.environment, frame);

    drawScene(m_scene, *m_models, frame.cameraPos, out);
    if (!m_started) return;
    drawWorld(m_previous, current, alpha, localPlayer, now, m_models->boxMesh(), m_view, out);
}

} // namespace sandbox

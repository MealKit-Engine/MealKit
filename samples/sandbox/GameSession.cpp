#include "GameSession.h"

#include "aster/core/Clock.h"
#include "aster/core/Profile.h"
#include "aster/platform/Window.h"

#include <algorithm>
#include <cmath>
#include <cstring>

using namespace aster;

namespace sandbox {
namespace {

constexpr f32 kFovY = 60.0f * 3.14159265f / 180.0f;

constexpr u32 kPlayerColors[net::kMaxPlayers] = {
    render::rgba(66, 135, 245), render::rgba(235, 64, 52),  render::rgba(52, 199, 89),  render::rgba(250, 204, 21),
    render::rgba(168, 85, 247), render::rgba(236, 72, 153), render::rgba(20, 184, 166), render::rgba(249, 115, 22),
};

i8 quantize(f32 v) { return static_cast<i8>(std::lround(std::clamp(v, -1.0f, 1.0f) * 127.0f)); }

// 스크립트 입력: 원을 그리며 걷고 주기적으로 밀치기/점프. 입력 바이트가 전송되므로 여기서는 삼각함수를 써도 된다.
GameInput botInput(u32 tick, u8 player) {
    GameInput g;
    const f32 a = static_cast<f32>(tick) * 0.05f + static_cast<f32>(player) * 1.7f;
    g.moveX = quantize(std::sin(a));
    g.moveZ = quantize(std::cos(a));
    if ((tick + player * 11u) % 40u == 0) g.buttons |= kButtonPush;
    if ((tick + player * 7u) % 75u == 0) g.buttons |= kButtonJump;
    return g;
}

} // namespace

void GameSession::start(const scene::Scene& scene, const net::LockstepSession& session, const game::PlayOptions& options,
                        asset::ModelCache& models) {
    m_scene = scene;
    m_options = options;
    m_models = &models;
    m_world.reset(session.seed(), session.playerCount(), session.matchData(), m_scene, models, options.physicsThreads);
    m_previous = m_world.state();
    m_started = true;
    m_finished = false;
    m_captureDone = m_captureDue = false;
    m_nextTick = 0;
    m_accumulator = 0.0;
    m_pushLatch = m_jumpLatch = false;
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

    // 카메라: 오른쪽 드래그로 회전, 휠로 줌 (시뮬레이션과 무관하므로 지연 없음)
    if (mouseAllowed && input.mousePressed(MouseButton::Right)) m_orbiting = true;
    if (!input.mouseDown(MouseButton::Right) || !window.focused()) m_orbiting = false;
    window.setCursorCaptured(m_orbiting);
    if (m_orbiting) {
        m_cameraYaw += input.mouseDeltaX() * 0.005f;
        m_cameraPitch = std::clamp(m_cameraPitch + input.mouseDeltaY() * 0.005f, -0.1f, 1.45f);
    }
    if (mouseAllowed) m_cameraDistance = std::clamp(m_cameraDistance - input.wheelDelta() * 2.0f, 4.0f, 80.0f);
    m_pushLatch |= mouseAllowed && input.mousePressed(MouseButton::Left);
    m_jumpLatch |= keyboardAllowed && input.keyPressed(key::Space);

    if (!m_started || m_finished) return;
    ASTER_ZONE("Simulation");

    // 고정 30Hz, 확정된 틱만 실행
    m_accumulator += dt;
    int budget = 8; // 한 프레임에 따라잡을 최대 틱
    const f64 simStart = nowSeconds();
    while (m_accumulator >= kTickDt && budget-- > 0 && session.canExecute(m_nextTick)) {
        m_previous = m_world.state();
        m_world.step(session.inputs(m_nextTick));
        session.tickExecuted(m_nextTick, m_world.hash());

        GameInput gi;
        if (m_options.bot) {
            gi = botInput(m_nextTick, session.localPlayer());
        } else if (keyboardAllowed || mouseAllowed) {
            const Vec3 forward{-std::sin(m_cameraYaw), 0.0f, -std::cos(m_cameraYaw)};
            const Vec3 right = cross(Vec3{0, 1, 0}, forward);
            Vec3 move{};
            if (keyboardAllowed) {
                if (input.keyDown('W')) move += forward;
                if (input.keyDown('S')) move -= forward;
                if (input.keyDown('D')) move += right;
                if (input.keyDown('A')) move -= right;
            }
            move = normalize(move);
            gi.moveX = quantize(move.x);
            gi.moveZ = quantize(move.z);
            if (m_jumpLatch || (keyboardAllowed && input.keyDown(key::Space))) gi.buttons |= kButtonJump;
            if (m_pushLatch) gi.buttons |= kButtonPush;
        }
        m_jumpLatch = m_pushLatch = false;
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
    const render::MeshId box = m_models->boxMesh();
    const f32 alpha = m_started ? std::clamp(static_cast<f32>(m_accumulator / kTickDt), 0.0f, 1.0f) : 1.0f;
    const WorldState& current = m_world.state();

    Vec3 focus{0.0f, 1.0f, 0.0f};
    if (m_started && localPlayer < current.playerCount)
        focus = lerp(m_previous.players[localPlayer].position, current.players[localPlayer].position, alpha) + Vec3{0, 0.8f, 0};
    const Vec3 eye = focus + Vec3{std::sin(m_cameraYaw) * std::cos(m_cameraPitch), std::sin(m_cameraPitch),
                                  std::cos(m_cameraYaw) * std::cos(m_cameraPitch)} *
                                 m_cameraDistance;

    const f32 width = viewport.width > 0.0f ? viewport.width : 1.0f;
    const f32 height = viewport.height > 0.0f ? viewport.height : 1.0f;
    frame.viewport = viewport;
    frame.viewProj = lookAtLH(eye, focus, Vec3{0, 1, 0}) * perspectiveReversedZ(kFovY, width / height, 0.1f, 1000.0f);
    frame.cameraPos = eye;
    const scene::Environment& env = m_scene.environment;
    frame.sunDirection = env.sunDirection;
    frame.sunColor = env.sunColor;
    frame.skyColor = env.skyColor;
    frame.groundColor = env.groundColor;
    frame.fogColor = env.fogColor;
    frame.fogDensity = env.fogDensity;

    for (const scene::Entity& e : m_scene.entities) {
        if (e.kind == scene::EntityKind::SpawnPoint) continue; // 게임에서는 보이지 않음
        const asset::Model* model = e.model.empty() ? nullptr : m_models->get(e.model);
        const render::MeshId mesh = model ? m_models->pickLod(*model, eye, e.position, e.scale) : box;
        out.add(mesh, {e.position, e.color, e.rotation, e.scale, e.flags});
    }
    if (!m_started) return;

    const size_t bodyCount = std::min(m_previous.bodies.size(), current.bodies.size());
    for (size_t i = 0; i < bodyCount; ++i) {
        const Body& a = m_previous.bodies[i];
        const Body& b = current.bodies[i];
        out.add(box, {lerp(a.position, b.position, alpha), b.color, nlerp(a.rotation, b.rotation, alpha), b.halfExtents * 2.0f, 0});
    }
    for (u32 p = 0; p < current.playerCount; ++p) {
        const Player& a = m_previous.players[p];
        const Player& b = current.players[p];
        const Vec3 pos = lerp(a.position, b.position, alpha);
        if (b.velocity.x * b.velocity.x + b.velocity.z * b.velocity.z > 0.25f) m_facing[p] = std::atan2(b.velocity.x, b.velocity.z);
        const Quat facing = quatFromAxisAngle({0, 1, 0}, m_facing[p]);
        out.add(box, {pos, kPlayerColors[p], facing, {0.8f, kPlayerHalfHeight * 2, 0.8f}, 0});
        out.add(box, {pos + rotate(facing, Vec3{0, 0.45f, 0.4f}), render::rgba(30, 30, 36), facing, {0.5f, 0.18f, 0.12f}, 0});
        if (p == localPlayer) { // 내 캐릭터 표시
            const Quat spin = quatFromAxisAngle({0, 1, 0}, static_cast<f32>(now) * 3.0f);
            out.add(box, {pos + Vec3{0, 1.6f, 0}, render::rgba(255, 230, 80), spin, {0.3f, 0.3f, 0.3f}, 0});
        }
    }
}

} // namespace sandbox

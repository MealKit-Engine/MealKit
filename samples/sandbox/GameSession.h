#pragma once

#include "World.h"

#include "aster/asset/ModelCache.h"
#include "aster/game/GameInstance.h"
#include "aster/net/Lockstep.h"
#include "aster/render/DrawList.h"
#include "aster/render/Renderer.h"
#include "aster/scene/Scene.h"

#include <string>
#include <vector>

// 한 판의 진행: 결정론 시뮬레이션 실행, 로컬 입력 샘플링, 3인칭 카메라, 장면 그리기.
// 출시용 게임(main.cpp)은 이 클래스를 직접 쓰고, 범용 에디터는 게임 DLL(GameModule.cpp)을 통해 쓴다.
namespace sandbox {

class GameSession final : public aster::game::GameInstance {
public:
    GameSession() = default;
    GameSession(const GameSession&) = delete;
    GameSession& operator=(const GameSession&) = delete;

    void start(const aster::scene::Scene& scene, const aster::net::LockstepSession& session,
               const aster::game::PlayOptions& options, aster::asset::ModelCache& models) override;
    bool started() const override { return m_started; }
    void update(aster::f64 dt, aster::net::LockstepSession& session, aster::Window& window, bool keyboardAllowed,
                bool mouseAllowed) override;
    void buildFrame(aster::f64 now, u8 localPlayer, aster::render::Viewport viewport, aster::render::FrameParams& frame,
                    aster::render::DrawList& out) override;

    u32 nextTick() const override { return m_nextTick; }
    u64 stateHash() const override { return m_world.hash(); }
    u32 activeBodies() const override { return m_world.activeBodies(); }
    bool finished() const override { return m_finished; }
    bool captureDue() override;
    aster::f64 takeSimMsPerTick() override;

private:
    World m_world;
    WorldState m_previous; // 렌더 보간용 직전 틱
    aster::scene::Scene m_scene;
    aster::asset::ModelCache* m_models = nullptr;
    aster::game::PlayOptions m_options;
    bool m_started = false;
    bool m_finished = false;
    bool m_captureDone = false;
    bool m_captureDue = false;
    u32 m_nextTick = 0;
    aster::f64 m_accumulator = 0.0;
    bool m_pushLatch = false;
    bool m_jumpLatch = false;
    bool m_orbiting = false;
    f32 m_cameraYaw = 0.8f;
    f32 m_cameraPitch = 0.55f;
    f32 m_cameraDistance = 18.0f;
    f32 m_facing[aster::net::kMaxPlayers] = {};
    aster::f64 m_simSeconds = 0.0;
    u32 m_simTicks = 0;
};

} // namespace sandbox

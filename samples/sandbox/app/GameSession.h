#pragma once

#include "sim/World.h"
#include "view/Camera.h"
#include "view/Draw.h"
#include "view/Effects.h"
#include "view/LocalInput.h"

#include "mealkit/asset/ModelCache.h"
#include "mealkit/game/GameInstance.h"
#include "mealkit/net/Lockstep.h"
#include "mealkit/render/DrawList.h"
#include "mealkit/render/Renderer.h"
#include "mealkit/scene/Scene.h"

#include <string>
#include <vector>

// 한 판의 진행: 확정된 틱마다 시뮬레이션(sim/)을 돌리고 로컬 입력을 보내며, 화면(view/)을 그린다.
// 출시용 게임(main.cpp)은 이 클래스를 직접 쓰고, 범용 에디터는 게임 DLL(GameModule.cpp)을 통해 쓴다.
namespace sandbox {

class GameSession final : public mealkit::game::GameInstance {
public:
    using TuningType = Tuning; // 에디터 [튜닝] 패널 (MEALKIT_GAME_MODULE이 읽는다)

    GameSession() = default;
    GameSession(const GameSession&) = delete;
    GameSession& operator=(const GameSession&) = delete;

    void start(mealkit::game::GameContext& ctx, const mealkit::scene::Scene& scene,
               const mealkit::game::PlayOptions& options) override;
    bool started() const override { return m_started; }
    void update(mealkit::game::GameContext& ctx, mealkit::f64 dt) override;
    void buildFrame(mealkit::game::GameContext& ctx, mealkit::f64 now, mealkit::render::FrameParams& frame,
                    mealkit::render::DrawList& out) override;

    u32 nextTick() const override { return m_nextTick; }
    u64 stateHash() const override { return m_world.hash(); }
    u32 activeBodies() const override { return m_world.activeBodies(); }
    bool finished() const override { return m_finished; }
    bool captureDue() override;
    mealkit::f64 takeSimMsPerTick() override;
    void setTuning(const void* tuning) override { m_world.setTuning(*static_cast<const Tuning*>(tuning)); }
    mealkit::reflect::View stateView() const override { return {&mealkit::reflect::typeOf<WorldState>(), &m_world.state}; }

private:
    World m_world;
    WorldState m_previous; // 렌더 보간용 직전 틱
    mealkit::scene::Scene m_scene;
    mealkit::game::PlayOptions m_options;
    bool m_started = false;
    bool m_finished = false;
    bool m_captureDone = false;
    bool m_captureDue = false;
    u32 m_nextTick = 0;
    mealkit::f64 m_accumulator = 0.0;
    OrbitCamera m_camera;
    LocalInput m_input;
    WorldView m_view;
    Effects m_effects;           // 틱 이벤트 → 화면 효과
    mealkit::f64 m_viewTime = 0.0; // 화면 효과용 시계 (update의 dt 합)
    mealkit::f64 m_simSeconds = 0.0;
    u32 m_simTicks = 0;
};

} // namespace sandbox

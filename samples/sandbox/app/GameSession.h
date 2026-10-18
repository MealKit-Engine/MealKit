#pragma once

#include "app/ProjectSettings.h"
#include "sim/World.h"
#include "view/Camera.h"
#include "view/Draw.h"
#include "view/Effects.h"
#include "view/Hud.h"
#include "view/LocalInput.h"
#include "view/ViewTuning.h"

#include "mealkit/asset/ModelCache.h"
#include "mealkit/audio/Audio.h"
#include "mealkit/game/GameInstance.h"
#include "mealkit/game/Simulation.h"
#include "mealkit/net/Lockstep.h"
#include "mealkit/render/DrawList.h"
#include "mealkit/render/Renderer.h"
#include "mealkit/scene/Scene.h"

#include <array>
#include <string>
#include <vector>

// 한 판의 진행: 확정된 틱마다 시뮬레이션(sim/)을 돌리고 로컬 입력을 보내며, 화면(view/)을 그린다.
// 출시용 게임(main.cpp)은 이 클래스를 직접 쓰고, 범용 에디터는 게임 DLL(GameModule.cpp)을 통해 쓴다.
namespace sandbox {

class GameSession final : public mealkit::game::GameInstance {
public:
    using TuningType = Tuning;                   // 에디터 [튜닝] 패널 (MEALKIT_GAME_MODULE이 읽는다)
    using ProjectSettingsType = ProjectSettings; // 에디터 [프로젝트 설정] 창의 게임 항목
    using ViewTuningType = ViewTuning;           // 화면용 튜닝 (assets/view.tuning, [튜닝] 패널 "화면" 탭)
    using EventType = GameEvent;                 // 틱 이벤트 ([연출] 패널, assets/feedback.table)

    GameSession() = default;
    GameSession(const GameSession&) = delete;
    GameSession& operator=(const GameSession&) = delete;

    void start(mealkit::game::GameContext& ctx, const mealkit::scene::Scene& scene,
               const mealkit::game::PlayOptions& options) override;
    bool started() const override { return m_sim.started(); }
    void update(mealkit::game::GameContext& ctx, mealkit::f64 dt) override;
    void buildFrame(mealkit::game::GameContext& ctx, mealkit::f64 now, mealkit::render::FrameParams& frame,
                    mealkit::render::DrawList& out) override;

    u32 nextTick() const override { return m_sim.nextTick(); }
    u64 stateHash() const override { return m_sim.world().hash(); }
    u32 activeBodies() const override { return m_sim.world().activeBodies(); }
    bool finished() const override { return m_sim.finished(); }
    bool captureDue() override { return m_sim.captureDue(); }
    mealkit::f64 takeSimMsPerTick() override { return m_sim.takeSimMsPerTick(); }
    void setTuning(const void* tuning) override;
    mealkit::reflect::View stateView() const override {
        return {&mealkit::reflect::typeOf<WorldState>(), &m_sim.world().state};
    }
    mealkit::game::SimDebug* debug() override { return &m_sim; }
    void debugUi(mealkit::game::GameContext& ctx) override;

private:
    // 고정 틱 실행, 보간용 직전 상태, 그림자 시뮬레이션, 기록·되감기 (엔진 mealkit/game/Simulation.h)
    mealkit::game::Simulation<World> m_sim;
    mealkit::scene::Scene m_scene;
    mealkit::game::PlayOptions m_options;
    OrbitCamera m_camera;
    LocalInput m_input;
    WorldView m_view;
    Vec3 m_focus{0.0f, 1.0f, 0.0f}; // 카메라가 따라가는 점 (내 Pawn이 없으면 마지막 위치)
    Effects m_effects;              // 틱 이벤트 → 화면 효과
    mealkit::f64 m_viewTime = 0.0;  // 화면 효과용 시계 (update의 dt 합)
    mealkit::audio::Handle m_music; // 배경음 (매치 동안)
    // 게임 화면 UI (assets/ui): HUD와 캐릭터 위 이름표 (월드 위 UI)
    mealkit::u32 m_hud = 0, m_nameTag = 0;
    HudVm m_hudVm;
    std::array<NameTagVm, mealkit::net::kMaxPlayers> m_tagVms; // drawAt이 그릴 때까지 살아 있어야 한다
    // 화면 정지(히트 스톱) 동안 그리는 상태: 시작한 순간의 두 틱 상태와 보간 비율 (시뮬레이션은 계속 돈다)
    bool m_frozen = false;
    WorldState m_frozenPrevious, m_frozenCurrent;
    f32 m_frozenAlpha = 1.0f;
};

} // namespace sandbox

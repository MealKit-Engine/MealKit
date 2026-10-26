#pragma once

#include "sim/World.h"
#include "view/ViewTuning.h"

#include "mealkit/asset/ModelCache.h"
#include "mealkit/render/DrawList.h"
#include "mealkit/render/Renderer.h"
#include "mealkit/scene/Scene.h"

#include <vector>

// 장면 그리기. 시뮬레이션 상태는 읽기만 한다.
namespace sandbox {

// 화면에만 있는 상태 (프레임 사이에 유지)
struct WorldView {
    std::vector<f32> facing;      // Pawn 슬롯마다 바라보는 방향 (라디안)
    std::vector<mealkit::f64> hitTime; // Pawn 슬롯마다 마지막으로 밀쳐진 화면 시각 (번쩍임, 없으면 음수)
    struct Flash {
        Vec3 position;
        mealkit::f64 time;
    };
    std::vector<Flash> flashes; // 밀치기 빛 (화면 시각, applyLights가 점광원으로)
};

// 틱 이벤트 → 화면 상태: 밀치기 범위 안의 다른 Pawn이 번쩍인다 (viewTime: 화면 정지 동안 멈추는 화면 시계)
void onWorldEvent(const GameEvent& e, const WorldState& state, mealkit::f64 viewTime, WorldView& view);

// 씬 환경 → 화면 (하늘 HDRI는 models가 굽거나 캐시에서 읽어 번호를 준다)
void applyEnvironment(const mealkit::scene::Environment& env, mealkit::asset::ModelCache& models, mealkit::render::FrameParams& frame);
// 씬의 점광원 + 밀치기 빛 → 화면 (lights는 frame이 가리킨다: 프레임 동안 살아 있어야 한다)
void applyLights(const mealkit::scene::Scene& scene, const WorldView& view, mealkit::f64 viewTime,
                 std::vector<mealkit::render::PointLight>& lights, mealkit::render::FrameParams& frame);
// 씬의 반사 프로브 → 화면 (probes는 frame이 가리킨다: 프레임 동안 살아 있어야 한다)
void applyProbes(const mealkit::scene::Scene& scene, mealkit::asset::ModelCache& models,
                 std::vector<mealkit::render::ReflectionProbe>& probes, mealkit::render::FrameParams& frame);
// 씬의 정적 물체 (스폰 지점과 영역은 그리지 않는다)
void drawScene(const mealkit::scene::Scene& scene, mealkit::asset::ModelCache& models, const Vec3& eye,
               mealkit::render::DrawList& out);
// 직전 틱과 현재 틱 사이를 alpha로 보간해 상자와 Pawn을 그린다. localOffset: 내 캐릭터만 더 옮겨 그린다 (미리 움직이기)
void drawWorld(const WorldState& previous, const WorldState& current, f32 alpha, u8 localPlayer, mealkit::f64 now,
               mealkit::f64 viewTime, const ViewTuning& look, mealkit::render::MeshId box, WorldView& view, mealkit::render::DrawList& out,
               const Vec3& localOffset = {});

} // namespace sandbox

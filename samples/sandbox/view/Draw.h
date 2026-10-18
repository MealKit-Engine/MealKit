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
    std::vector<f32> facing; // Pawn 슬롯마다 바라보는 방향 (라디안)
};

void applyEnvironment(const mealkit::scene::Environment& env, mealkit::render::FrameParams& frame);
// 씬의 정적 물체 (스폰 지점과 영역은 그리지 않는다)
void drawScene(const mealkit::scene::Scene& scene, mealkit::asset::ModelCache& models, const Vec3& eye,
               mealkit::render::DrawList& out);
// 직전 틱과 현재 틱 사이를 alpha로 보간해 상자와 Pawn을 그린다
void drawWorld(const WorldState& previous, const WorldState& current, f32 alpha, u8 localPlayer, mealkit::f64 now,
               const ViewTuning& look, mealkit::render::MeshId box, WorldView& view, mealkit::render::DrawList& out);

} // namespace sandbox

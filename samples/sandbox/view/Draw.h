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
};

// 틱 이벤트 → 화면 상태: 밀치기 범위 안의 다른 Pawn이 번쩍인다 (viewTime: 화면 정지 동안 멈추는 화면 시계)
void onWorldEvent(const GameEvent& e, const WorldState& state, mealkit::f64 viewTime, WorldView& view);

void applyEnvironment(const mealkit::scene::Environment& env, mealkit::render::FrameParams& frame);
// 씬의 정적 물체 (스폰 지점과 영역은 그리지 않는다)
void drawScene(const mealkit::scene::Scene& scene, mealkit::asset::ModelCache& models, const Vec3& eye,
               mealkit::render::DrawList& out);
// 직전 틱과 현재 틱 사이를 alpha로 보간해 상자와 Pawn을 그린다. localOffset: 내 캐릭터만 더 옮겨 그린다 (미리 움직이기)
void drawWorld(const WorldState& previous, const WorldState& current, f32 alpha, u8 localPlayer, mealkit::f64 now,
               mealkit::f64 viewTime, const ViewTuning& look, mealkit::render::MeshId box, WorldView& view, mealkit::render::DrawList& out,
               const Vec3& localOffset = {});

} // namespace sandbox

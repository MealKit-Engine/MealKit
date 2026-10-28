#pragma once

#include "mealkit/render/DrawList.h"
#include "mealkit/render/Renderer.h"

// 쌓인 디버그 그리기 요청(debug/DebugDraw.h)을 그린다: 선·상자는 얇은 상자 인스턴스(깊이 있음),
// 글자는 ImGui 앞 그리기 목록(ImGui 프레임 안일 때만, drawText). 게임 exe와 에디터 플레이가 buildFrame 다음에 부른다.
namespace mealkit::debug {

void flushDebugDraw(render::DrawList& out, render::MeshId box, const render::FrameParams& frame, f64 now, bool drawText);

} // namespace mealkit::debug

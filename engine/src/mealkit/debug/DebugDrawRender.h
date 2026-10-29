#pragma once

#include "mealkit/render/DrawList.h"
#include "mealkit/render/Renderer.h"

// Draws queued debug draw requests (debug/DebugDraw.h): lines/boxes as thin box instances (with depth),
// text via ImGui's foreground draw list (only inside an ImGui frame, drawText). Called by the game exe and editor play after buildFrame.
namespace mealkit::debug {

void flushDebugDraw(render::DrawList& out, render::MeshId box, const render::FrameParams& frame, f64 now, bool drawText);

} // namespace mealkit::debug

// Vendored copy of module-render v0.6.2; change it in that repo (tools\module.ps1 update), not here.
#pragma once

#include "mealkit/render/Renderer.h"

#include <mkrender/DrawList.h>

// The per-frame draw list is mkrender's (MkRender): instances per mesh, skinned meshes, sprites and particles, culled on the job workers
// through render::Renderer (RendererDesc::parallelFor) and drawn by submit
namespace mealkit::render {

using namespace ::mkrender;

} // namespace mealkit::render

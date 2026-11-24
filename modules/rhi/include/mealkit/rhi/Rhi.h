// Vendored copy of module-rhi v0.2.0; change it in that repo (tools\module.ps1 update), not here.
#pragma once

#include "mealkit/core/Types.h"

#include <mkrhi/Rhi.h>

// Engine glue for mkrhi (MkRender, third_party/mkrender): the graphics API layer itself is the engine-free library;
// mealkit::rhi names it for engine code (rhi::Device, rhi::Texture ...) and adds what needs the engine (GpuCapture.h, logging).
// Graphics API headers stay inside MkRender's backend folders (tools\check_platform.ps1).
namespace mealkit::rhi {

using namespace ::mkrhi;

// Routes mkrhi messages into the engine log (MEALKIT_INFO/WARN/ERROR, fatal → log::fatal). Called before the device is made
// (render::Renderer::init); calling it again does nothing
void connectLog();

} // namespace mealkit::rhi

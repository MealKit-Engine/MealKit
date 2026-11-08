#pragma once

#include "mealkit/core/Types.h"

#include <span>

// Engine modules built into this executable, in dependency order (docs/ENGINE_MODULES.md). The list is generated at configure
// time from the engine-module.txt manifests (engine/CMakeLists.txt): each module comes after everything it requires, ties by name.
// That's the order owners create services in and the reverse is the order they go away (core/Services.h). Printed by --modules.
namespace mealkit {

struct EngineModuleInfo {
    const char* name;
    const char* version;
    const char* kind; // kernel, sim, view, tool
};

std::span<const EngineModuleInfo> engineModules();
void logEngineModules(); // one log line per module, then the services provided right now

} // namespace mealkit

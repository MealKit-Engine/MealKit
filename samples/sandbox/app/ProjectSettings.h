#pragma once

#include "mealkit/core/Reflect.h"

// Project settings the game adds: game.* lines in mealkit.project (edited in the editor's [File] > [Project Settings]).
// Read once when the game executable starts. Game rule values go in Tuning (game.tuning), not here.
// Items to exclude from the content hash (values that don't touch the simulation) are declared with MEALKIT_VIEW_FIELD.
namespace sandbox {

MEALKIT_STRUCT(ProjectSettings) {
    MEALKIT_VIEW_FIELD(mealkit::u16, defaultPort, 27015, "기본 UDP 포트 (--host, --join)");
    MEALKIT_VIEW_FIELD(mealkit::u32, defaultMaxPlayers, 4, "방을 만들 때 기본 최대 인원 (최대 8)");
};

} // namespace sandbox

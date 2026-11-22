// Precompiled header for game targets (mealkit_add_game, Game and Editor builds): the standard and engine headers that most game files include,
// so each .cpp doesn't parse them again. Applied only to view/ and app/ files: sim/ files and app/Headless.cpp are skipped (see mealkit_add_game),
// because they also build in <game>_headless against the kernel only, and a force-included header must not hide a missing include there.
// Engine module headers sit behind MEALKIT_HAS_<NAME> like game code does. Keep this list to headers that are cheap to be wrong about:
// anything here is part of every view/app file's include set (and a change to it rebuilds them all).
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "mealkit/core/Reflect.h"
#include "mealkit/core/Types.h"
#include "mealkit/platform/Window.h"

#if MEALKIT_HAS_GAME
#include "mealkit/game/GameContext.h"
#include "mealkit/game/GameData.h"
#include "mealkit/game/GameInstance.h"
#include "mealkit/game/Loadout.h"
#include "mealkit/game/MatchService.h"
#endif
#if MEALKIT_HAS_LOC
#include "mealkit/loc/Strings.h"
#endif
#if MEALKIT_HAS_SETTINGS
#include "mealkit/settings/Settings.h"
#endif
#if MEALKIT_HAS_SIM
#include "mealkit/sim/Commands.h"
#endif
#if MEALKIT_HAS_UI
#include "mealkit/ui/Ui.h"
#endif

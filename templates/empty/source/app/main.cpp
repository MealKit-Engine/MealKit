// Entry point of the shipping game executable. Window, session, view-side services, and automated test output are handled by the engine (mealkit/game/GameMain.h, --help).
// All this game decides is GameSession (running a match) and Headless (match start, bot input, default port and player count).
//
// Controls: WASD move, Space jump, right drag camera, wheel zoom (assets/input.bindings; engine keys: V vsync, F12 screenshot)

#include "app/GameSession.h"
#include "app/Headless.h"

#include "mealkit/game/GameMain.h"

MEALKIT_GAME_MAIN(sandbox::GameSession, sandbox::Headless)

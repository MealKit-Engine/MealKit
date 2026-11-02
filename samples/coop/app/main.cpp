// Entry point of the shipping game executable. Window, session, view-side services, and automated test output are handled by the engine (mealkit/game/GameMain.h, --help).
// All this game decides is GameSession (running a match) and Headless (match start, bot input, default port and player count).
//
// Controls: WASD move, Space jump, left click shove (stuns chasers), right drag camera, wheel zoom, F8 debug: stun every chaser (engine keys: V vsync, F12 screenshot, Esc quit)

#include "app/GameSession.h"
#include "app/Headless.h"

#include "mealkit/game/GameMain.h"

MEALKIT_GAME_MAIN(coop::GameSession, coop::Headless)

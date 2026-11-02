// Entry point of the game DLL loaded by the general-purpose editor (MealKitEditor). Not included in the shipping exe.
#include "app/GameSession.h"

MEALKIT_GAME_MODULE(MEALKIT_GAME_NAME,
                    coop::GameSession) // MEALKIT_GAME_NAME: project name passed by mealkit_add_game

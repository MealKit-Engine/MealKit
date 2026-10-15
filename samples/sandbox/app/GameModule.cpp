// 범용 에디터(MealKitEditor)가 불러오는 게임 DLL의 진입점. 출시용 exe에는 들어가지 않는다.
#include "app/GameSession.h"

MEALKIT_GAME_MODULE(MEALKIT_GAME_NAME,
                    sandbox::GameSession) // MEALKIT_GAME_NAME: mealkit_add_game이 넘기는 프로젝트 이름

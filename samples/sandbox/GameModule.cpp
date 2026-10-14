// 범용 에디터(AsterEditor)가 불러오는 게임 DLL의 진입점. 출시용 exe에는 들어가지 않는다.
#include "GameSession.h"

ASTER_GAME_MODULE(ASTER_GAME_NAME, sandbox::GameSession) // ASTER_GAME_NAME: aster_add_game이 넘기는 프로젝트 이름

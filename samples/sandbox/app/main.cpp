// 출시용 게임 실행 파일의 진입점. 창·세션·화면 쪽 서비스·자동 시험 출력은 엔진이 한다 (mealkit/game/GameMain.h, --help).
// 이 게임이 정하는 것은 GameSession(한 판 진행)과 Headless(매치 시작·봇 입력·기본 포트와 인원)뿐이다.
//
// 조작: WASD 이동, Space 점프, 왼쪽 클릭 밀치기, 오른쪽 드래그 카메라, 휠 줌, F8 디버그 충격파 (엔진 키: V 수직동기, F12 스크린샷, Esc 종료)

#include "app/GameSession.h"
#include "app/Headless.h"

#include "mealkit/game/GameMain.h"

MEALKIT_GAME_MAIN(sandbox::GameSession, sandbox::Headless)

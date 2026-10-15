# 엔진 저장소의 예제 게임 (프로젝트 폴더 = 엔진 저장소 최상위, mealkit.project).
#   Game 빌드:   sandbox.exe (출시용, 결정론 테스트 mp_test.ps1이 쓴다)
#   Editor 빌드: sandbox_game.dll (MealKitEditor가 불러온다)
# 새 게임 프로젝트의 템플릿이기도 하다 (tools\new_project.ps1이 이 폴더의 코드를 복사한다).
mealkit_add_game(sandbox
    SOURCES World.h World.cpp GameSession.h GameSession.cpp
    GAME_MAIN main.cpp
    MODULE GameModule.cpp)

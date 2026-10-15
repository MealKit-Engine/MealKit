# 엔진 저장소의 예제 게임 (프로젝트 폴더 = 엔진 저장소 최상위, mealkit.project).
#   Game 빌드:   sandbox.exe (출시용, 결정론 테스트 mp_test.ps1이 쓴다)
#   Editor 빌드: sandbox_game.dll (MealKitEditor가 불러온다)
# 새 게임 프로젝트의 템플릿이기도 하다 (tools/new_project.ps1이 sim/ view/ app/ 을 프로젝트의 source/로 복사한다).
#   sim/  결정론 시뮬레이션 (빌드 때 SimCheck가 규칙을 검사한다)
#   view/ 카메라, 로컬 입력, 그리기
#   app/  main(출시 exe), GameSession(한 판 진행), GameModule(에디터 진입점)
mealkit_add_game(sandbox SOURCE_DIR .)

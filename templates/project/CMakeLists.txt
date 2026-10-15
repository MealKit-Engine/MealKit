cmake_minimum_required(VERSION 3.25)
project(@NAME@ LANGUAGES CXX)

# MealKit 엔진 폴더: build.bat이 mealkit.project의 engine= 값을 -DMEALKIT_ROOT로 넘긴다.
file(TO_CMAKE_PATH "${MEALKIT_ROOT}" MEALKIT_ROOT)
if(NOT MEALKIT_ROOT OR NOT EXISTS "${MEALKIT_ROOT}/cmake/MealKit.cmake")
    message(FATAL_ERROR "MealKit engine not found (MEALKIT_ROOT='${MEALKIT_ROOT}'). Build with build.bat.")
endif()
include(${MEALKIT_ROOT}/cmake/MealKit.cmake)

# build.bat (Release)  → @NAME@.exe        출시용 게임 (엔진 정적 링크)
# build.bat Editor     → @NAME@_game.dll   에디터가 불러오는 게임 코드 (핫 리로드)
# 게임 코드는 source/ 아래 세 폴더에 둔다. 파일을 추가해도 이 파일을 고치지 않는다.
#   sim/  결정론 시뮬레이션. 빌드 때 SimCheck가 규칙(삼각함수·시간·rand·렌더러 헤더·전역 상태 금지 등)을 검사한다
#   view/ 카메라, 로컬 입력, 그리기 (sim/ 상태는 읽기만)
#   app/  main.cpp(출시 exe), GameModule.cpp(에디터 진입점), GameSession(한 판 진행)
mealkit_add_game(@NAME@
    STEAM_APP_ID 480) # 480 = Valve 테스트 앱(Spacewar). Steam에서 받은 앱 ID로 바꾼다.

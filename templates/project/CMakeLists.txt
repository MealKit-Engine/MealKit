cmake_minimum_required(VERSION 3.25)
project(@NAME@ LANGUAGES CXX)

# Aster 엔진 폴더: build.bat이 aster.project의 engine= 값을 -DASTER_ROOT로 넘긴다.
file(TO_CMAKE_PATH "${ASTER_ROOT}" ASTER_ROOT)
if(NOT ASTER_ROOT OR NOT EXISTS "${ASTER_ROOT}/cmake/Aster.cmake")
    message(FATAL_ERROR "Aster engine not found (ASTER_ROOT='${ASTER_ROOT}'). Build with build.bat.")
endif()
include(${ASTER_ROOT}/cmake/Aster.cmake)

# build.bat (Release)  → @NAME@.exe        출시용 게임 (엔진 정적 링크)
# build.bat Editor     → @NAME@_game.dll   에디터가 불러오는 게임 코드 (핫 리로드)
# 게임 코드 파일을 추가하면 SOURCES에 넣는다.
aster_add_game(@NAME@
    SOURCES
        source/World.h
        source/World.cpp
        source/GameSession.h
        source/GameSession.cpp
    GAME_MAIN source/main.cpp
    MODULE source/GameModule.cpp
    STEAM_APP_ID 480) # 480 = Valve 테스트 앱(Spacewar). Steam에서 받은 앱 ID로 바꾼다.

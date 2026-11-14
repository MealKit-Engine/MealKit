cmake_minimum_required(VERSION 3.25)
project(@NAME@ LANGUAGES CXX)

# MealKit engine folder: build.bat passes the engine= value from mealkit.project as -DMEALKIT_ROOT.
# IDEs configuring through CMakePresets.json don't, so read the same line here.
if(NOT MEALKIT_ROOT)
    file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/mealkit.project" _mk_engine REGEX "^[ \t]*engine[ \t=]")
    if(_mk_engine)
        string(REGEX REPLACE "^[ \t]*engine[ \t=]+[ \t]*" "" _mk_engine "${_mk_engine}")
        string(STRIP "${_mk_engine}" _mk_engine)
        set(MEALKIT_ROOT "${_mk_engine}" CACHE PATH "MealKit engine folder")
    endif()
endif()
file(TO_CMAKE_PATH "${MEALKIT_ROOT}" MEALKIT_ROOT)
if(NOT MEALKIT_ROOT OR NOT EXISTS "${MEALKIT_ROOT}/cmake/MealKit.cmake")
    message(FATAL_ERROR "MealKit engine not found (MEALKIT_ROOT='${MEALKIT_ROOT}'). Build with build.bat.")
endif()
include(${MEALKIT_ROOT}/cmake/MealKit.cmake)

# build.bat (Release)  → @NAME@.exe        shipping game (engine statically linked)
# build.bat Editor     → @NAME@_game.dll   game code loaded by the editor (hot reload)
# Game code goes in three folders under source/. Adding files doesn't require editing this file.
#   sim/  deterministic simulation. SimCheck checks the rules at build time (no trig, time, rand, renderer headers, global state, etc.)
#   view/ camera, local input, drawing (sim/ state is read only)
#   app/  main.cpp (shipping exe, one MEALKIT_GAME_MAIN line), GameModule.cpp (editor entry point), GameSession (runs one match)
mealkit_add_game(@NAME@
    STEAM_APP_ID 480) # 480 = Valve test app (Spacewar). Replace with the app ID you got from Steam.

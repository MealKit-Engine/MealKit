# Aster 엔진을 CMake 프로젝트에 붙인다. 엔진 저장소(D:\Aster)와 게임 프로젝트 폴더가 같이 쓴다.
#
#   cmake_minimum_required(VERSION 3.25)
#   project(MyGame LANGUAGES CXX)
#   include(${ASTER_ROOT}/cmake/Aster.cmake)
#   aster_add_game(MyGame SOURCES ... GAME_MAIN main.cpp MODULE GameModule.cpp)
#
# 빌드 종류 (ASTER_BUILD_KIND, build_project.bat이 정한다):
#   Game    출시용: 엔진을 정적 라이브러리로 넣은 <이름>.exe 하나 (정적 CRT). build\Release, build\Debug, build\Profile
#   Editor  에디터용: 엔진은 aster.dll(동적 CRT), 게임 코드는 <이름>_game.dll. build\Editor
#           엔진 저장소는 aster.dll과 AsterEditor.exe를 만들고, 게임 프로젝트는 그것을 가져와(AsterTargets.cmake) 게임 DLL만 만든다.
#
# 최상위 CMakeLists.txt에서 include해야 한다: 아래 전역 설정(C++20, CRT)이 게임 타깃에도 적용되어야 링크된다.
include_guard(GLOBAL)

if(NOT MSVC)
    message(FATAL_ERROR "Aster는 Windows + MSVC 전용입니다.")
endif()

get_filename_component(ASTER_ENGINE_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
get_filename_component(aster_source_dir "${CMAKE_SOURCE_DIR}" ABSOLUTE)
if(aster_source_dir STREQUAL ASTER_ENGINE_DIR)
    set(ASTER_IS_ENGINE ON)
else()
    set(ASTER_IS_ENGINE OFF)
endif()

set(ASTER_BUILD_KIND "Game" CACHE STRING "Game: 출시용 정적 exe / Editor: aster.dll + AsterEditor + 게임 DLL")
set_property(CACHE ASTER_BUILD_KIND PROPERTY STRINGS Game Editor)
if(NOT ASTER_BUILD_KIND MATCHES "^(Game|Editor)$")
    message(FATAL_ERROR "ASTER_BUILD_KIND must be Game or Editor (got '${ASTER_BUILD_KIND}')")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if(ASTER_BUILD_KIND STREQUAL "Editor")
    # 에디터와 게임 DLL이 엔진(aster.dll)과 힙을 같이 쓰도록 동적 CRT. 링크 시간 최적화는 끈다 (게임 DLL 다시 빌드를 빠르게).
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
else()
    # 정적 CRT: 재배포 패키지 없이 실행되고, 모든 PC가 같은 CRT 코드를 쓰므로 결정론에도 유리하다.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
endif()

option(ASTER_PROFILE "Tracy 프로파일링 마커 켜기 (build.bat Profile)" OFF)

if(ASTER_BUILD_KIND STREQUAL "Editor" AND NOT ASTER_IS_ENGINE)
    # ---------------------------------------------------------------- 게임 프로젝트의 에디터 빌드: 엔진은 가져다 쓴다
    set(ASTER_EDITOR_SDK "${ASTER_ENGINE_DIR}/build/Editor" CACHE PATH "엔진 에디터 빌드 폴더 (aster.dll, AsterEditor.exe)")
    if(NOT EXISTS "${ASTER_EDITOR_SDK}/AsterTargets.cmake")
        message(FATAL_ERROR "Engine editor build not found in ${ASTER_EDITOR_SDK}. Run ${ASTER_ENGINE_DIR}\\build.bat Editor first.")
    endif()
    include("${ASTER_EDITOR_SDK}/AsterTargets.cmake")
    set(ASTER_STEAM_DLL "")
else()
    # ---------------------------------------------------------------- 엔진을 소스에서 빌드

    # Jolt Physics: 결정론 빌드. 사무용 CPU(펜티엄/셀러론 포함)에서도 돌도록 SSE4.2까지만 쓴다.
    # 같은 실행 파일은 어느 CPU에서나 같은 명령어를 실행하므로 결정론도 유지된다.
    foreach(jolt_off
            TARGET_UNIT_TESTS TARGET_HELLO_WORLD TARGET_PERFORMANCE_TEST TARGET_SAMPLES TARGET_VIEWER
            ENABLE_INSTALL ENABLE_ALL_WARNINGS OVERRIDE_CXX_FLAGS GENERATE_DEBUG_SYMBOLS
            USE_AVX USE_AVX2 USE_AVX512 USE_LZCNT USE_TZCNT USE_F16C USE_FMADD
            DEBUG_RENDERER_IN_DEBUG_AND_RELEASE PROFILER_IN_DEBUG_AND_RELEASE ENABLE_OBJECT_STREAM
            FLOATING_POINT_EXCEPTIONS_ENABLED JPH_USE_DX12 JPH_USE_VK JPH_USE_MTL JPH_USE_CPU_COMPUTE)
        set(${jolt_off} OFF CACHE BOOL "" FORCE)
    endforeach()
    set(CROSS_PLATFORM_DETERMINISTIC ON CACHE BOOL "" FORCE)
    set(USE_SSE4_1 ON CACHE BOOL "" FORCE)
    set(USE_SSE4_2 ON CACHE BOOL "" FORCE)
    # Jolt는 기본으로 정적 CRT를 강제한다. 에디터 빌드에서는 aster.dll과 같은 동적 CRT로 맞춘다.
    if(ASTER_BUILD_KIND STREQUAL "Editor")
        set(USE_STATIC_MSVC_RUNTIME_LIBRARY OFF CACHE BOOL "" FORCE)
    else()
        set(USE_STATIC_MSVC_RUNTIME_LIBRARY ON CACHE BOOL "" FORCE)
    endif()
    add_subdirectory(${ASTER_ENGINE_DIR}/third_party/JoltPhysics/Build ${CMAKE_BINARY_DIR}/third_party/jolt EXCLUDE_FROM_ALL)

    # Tracy: 마커 헤더는 항상 쓰고, 클라이언트는 프로파일 빌드에서만 링크한다.
    if(ASTER_PROFILE)
        add_library(aster_tracy STATIC ${ASTER_ENGINE_DIR}/third_party/tracy/public/TracyClient.cpp)
        # 프로파일러가 붙었을 때만 수집하고, 127.0.0.1에서만 접속을 받는다 (방화벽 창 방지)
        target_compile_definitions(aster_tracy PUBLIC TRACY_ENABLE TRACY_ON_DEMAND TRACY_ONLY_LOCALHOST)
        target_compile_options(aster_tracy PRIVATE /W0)
        target_link_libraries(aster_tracy PUBLIC ws2_32 dbghelp)
    else()
        add_library(aster_tracy INTERFACE)
    endif()
    target_include_directories(aster_tracy SYSTEM INTERFACE ${ASTER_ENGINE_DIR}/third_party/tracy/public)

    add_subdirectory(${ASTER_ENGINE_DIR}/engine ${CMAKE_BINARY_DIR}/engine)
endif()

# ---------------------------------------------------------------- 게임

# aster_add_game(<이름>
#     SOURCES <게임 코드...>          게임과 에디터가 같이 쓰는 코드 (World, GameSession 등)
#     GAME_MAIN <main.cpp>            출시용 게임 실행 파일 <이름>.exe (Game 빌드)
#     MODULE <GameModule.cpp>         ASTER_GAME_MODULE이 있는 파일 → <이름>_game.dll (Editor 빌드)
#     [STEAM_APP_ID <id>])            기본 480 (Valve 테스트 앱 Spacewar)
#
# 프로젝트 폴더는 최상위 CMakeLists.txt가 있는 곳이다. 그 아래 assets/를 쓰고, 출시 빌드는 export/<이름>/에 만든다.
# game_export 타깃: 게임 실행 파일 + assets + Steam DLL + 라이선스 고지를 export/<이름>/에 모은다 (build.bat Export).
function(aster_add_game name)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "" "GAME_MAIN;MODULE;STEAM_APP_ID" "SOURCES")
    if(NOT ARG_GAME_MAIN OR NOT ARG_MODULE)
        message(FATAL_ERROR "aster_add_game(${name}): GAME_MAIN and MODULE are required")
    endif()
    if(NOT ARG_STEAM_APP_ID)
        set(ARG_STEAM_APP_ID 480)
    endif()
    set(project_dir ${CMAKE_SOURCE_DIR})

    if(ASTER_BUILD_KIND STREQUAL "Editor")
        # 에디터가 불러오는 게임 코드. 엔진은 aster.dll에서 가져온다.
        add_library(${name}_game SHARED ${ARG_MODULE} ${ARG_SOURCES})
        target_link_libraries(${name}_game PRIVATE aster)
        target_compile_definitions(${name}_game PRIVATE ASTER_GAME_NAME="${name}")
        return()
    endif()

    # 출시용 게임: 에디터와 ImGui가 들어가지 않는다.
    add_executable(${name} ${ARG_GAME_MAIN} ${ARG_SOURCES})
    target_link_libraries(${name} PRIVATE aster)
    target_compile_definitions(${name} PRIVATE ASTER_GAME_NAME="${name}")
    if(ASTER_STEAM_DLL)
        add_custom_command(TARGET ${name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${ASTER_STEAM_DLL} $<TARGET_FILE_DIR:${name}>)
        # 개발 중 실행용. 출시할 때는 내 앱 ID로 바꾸고, 480이 아니면 출시 폴더에 넣지 않는다.
        file(WRITE ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/steam_appid.txt "${ARG_STEAM_APP_ID}")
    endif()

    add_custom_target(game_export
        COMMAND ${CMAKE_COMMAND}
            -DGAME_EXE=$<TARGET_FILE:${name}>
            -DASSETS_DIR=${project_dir}/assets
            -DOUT_DIR=${project_dir}/export/${name}
            -DSTEAM_DLL=${ASTER_STEAM_DLL}
            -DSTEAM_APP_ID=${ARG_STEAM_APP_ID}
            -DNOTICES=${ASTER_ENGINE_DIR}/THIRD_PARTY_NOTICES.txt
            -P ${ASTER_ENGINE_DIR}/cmake/AsterExport.cmake
        DEPENDS ${name}
        COMMENT "Exporting ${name}"
        VERBATIM)
endfunction()

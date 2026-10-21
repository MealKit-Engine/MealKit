# MealKit 엔진을 CMake 프로젝트에 붙인다. 엔진 저장소(D:\MealKit)와 게임 프로젝트 폴더가 같이 쓴다.
#
#   cmake_minimum_required(VERSION 3.25)
#   project(MyGame LANGUAGES CXX)
#   include(${MEALKIT_ROOT}/cmake/MealKit.cmake)
#   mealkit_add_game(MyGame)   # source/sim, source/view, source/app
#
# 빌드 종류 (MEALKIT_BUILD_KIND, build_project.bat이 정한다):
#   Game    출시용: 엔진을 정적 라이브러리로 넣은 <이름>.exe 하나 (정적 CRT). build\Release, build\Debug, build\Profile
#   Editor  에디터용: 엔진은 mealkit.dll(동적 CRT), 게임 코드는 <이름>_game.dll. build\Editor
#           엔진 저장소는 mealkit.dll과 MealKitEditor.exe를 만들고, 게임 프로젝트는 그것을 가져와(MealKitTargets.cmake) 게임 DLL만 만든다.
#
# 최상위 CMakeLists.txt에서 include해야 한다: 아래 전역 설정(C++20, CRT)이 게임 타깃에도 적용되어야 링크된다.
include_guard(GLOBAL)

# Windows는 MSVC(또는 clang-cl). Linux·macOS는 Clang·GCC로 헤드리스 타깃(<게임>_headless, 엔진 단위 테스트)만 빌드한다 (CI 결정론 비교, 전체 이식은 ROADMAP 5단계).
if(WIN32)
    if(NOT MSVC)
        message(FATAL_ERROR "MealKit on Windows needs MSVC or clang-cl.")
    endif()
    set(MEALKIT_HEADLESS_ONLY OFF)
else()
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        message(FATAL_ERROR "MealKit headless builds need Clang or GCC (got ${CMAKE_CXX_COMPILER_ID}).")
    endif()
    set(MEALKIT_HEADLESS_ONLY ON)
endif()

get_filename_component(MEALKIT_ENGINE_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
# 엔진 버전 (저장소 루트 VERSION). 프로젝트의 mealkit.project engineVersion과 비교한다.
file(STRINGS "${MEALKIT_ENGINE_DIR}/VERSION" MEALKIT_VERSION LIMIT_COUNT 1)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${MEALKIT_ENGINE_DIR}/VERSION")
# 엔진 메시지가 가리키는 문서 주소 (문서 저장소 MealKit-docs, 공개 전에는 아직 열리지 않는다).
# 1.0 전에는 문서 가지가 master 하나라 latest, 1.0부터 엔진 MINOR 버전의 문서 가지 (docs/DECISIONS.md 2026-10-21)
string(REGEX MATCH "^[0-9]+\.[0-9]+" MEALKIT_VERSION_MINOR "${MEALKIT_VERSION}")
if(MEALKIT_VERSION VERSION_LESS 1.0)
    set(MEALKIT_DOCS_URL "https://mealkit.readthedocs.io/en/latest/")
else()
    set(MEALKIT_DOCS_URL "https://mealkit.readthedocs.io/en/${MEALKIT_VERSION_MINOR}/")
endif()
get_filename_component(mealkit_source_dir "${CMAKE_SOURCE_DIR}" ABSOLUTE)
if(mealkit_source_dir STREQUAL MEALKIT_ENGINE_DIR)
    set(MEALKIT_IS_ENGINE ON)
else()
    set(MEALKIT_IS_ENGINE OFF)
endif()

set(MEALKIT_BUILD_KIND "Game" CACHE STRING "Game: 출시용 정적 exe / Editor: mealkit.dll + MealKitEditor + 게임 DLL")
set_property(CACHE MEALKIT_BUILD_KIND PROPERTY STRINGS Game Editor)
if(NOT MEALKIT_BUILD_KIND MATCHES "^(Game|Editor)$")
    message(FATAL_ERROR "MEALKIT_BUILD_KIND must be Game or Editor (got '${MEALKIT_BUILD_KIND}')")
endif()
if(MEALKIT_HEADLESS_ONLY AND MEALKIT_BUILD_KIND STREQUAL "Editor")
    message(FATAL_ERROR "The editor build is Windows only (other platforms: ROADMAP stage 5).")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if(MEALKIT_BUILD_KIND STREQUAL "Editor")
    # 에디터와 게임 DLL이 엔진(mealkit.dll)과 힙을 같이 쓰도록 동적 CRT. 링크 시간 최적화는 끈다 (게임 DLL 다시 빌드를 빠르게).
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
else()
    # 정적 CRT: 재배포 패키지 없이 실행되고, 모든 PC가 같은 CRT 코드를 쓰므로 결정론에도 유리하다.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    # clang-cl 빌드(build.bat ClangCL)는 결정론 비교용이라 링크 시간 최적화를 쓰지 않는다 (lld-link가 필요해진다).
    if(MSVC AND NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
    endif()
endif()

# 그래픽 API 백엔드 (mealkit/rhi/<api>/, 빌드 때 하나). 출시 Windows 빌드는 DX12, Vulkan은 build.bat Vulkan·VulkanEditor.
set(MEALKIT_RHI "DX12" CACHE STRING "RHI 백엔드: DX12 / Vulkan")
set_property(CACHE MEALKIT_RHI PROPERTY STRINGS DX12 Vulkan)
if(NOT MEALKIT_RHI MATCHES "^(DX12|Vulkan)$")
    message(FATAL_ERROR "MEALKIT_RHI must be DX12 or Vulkan (got '${MEALKIT_RHI}')")
endif()

option(MEALKIT_PROFILE "Tracy 프로파일링 마커 켜기 (build.bat Profile)" OFF)
# 디버그 UI (Dear ImGui): 개발 빌드(build\Release·Debug·Profile)와 에디터에 넣고, 출시 빌드(build.bat Export → build\Export)는 뺀다.
# 켜지면 엔진 타깃이 MEALKIT_DEBUG_UI=1과 imgui.h 경로를 게임에 넘긴다. 게임 코드는 #if MEALKIT_DEBUG_UI로 감싼다.
option(MEALKIT_DEBUG_UI "디버그 UI(Dear ImGui) 넣기 (build.bat Export는 OFF)" ON)
if(MEALKIT_HEADLESS_ONLY)
    set(MEALKIT_DEBUG_UI OFF)
elseif(MEALKIT_BUILD_KIND STREQUAL "Editor")
    set(MEALKIT_DEBUG_UI ON) # 에디터는 ImGui로 그린다
endif()

if(MEALKIT_BUILD_KIND STREQUAL "Editor" AND NOT MEALKIT_IS_ENGINE)
    # ---------------------------------------------------------------- 게임 프로젝트의 에디터 빌드: 엔진은 가져다 쓴다
    # Vulkan 에디터(build.bat VulkanEditor)는 엔진의 build\VulkanEditor를 쓴다
    if(MEALKIT_RHI STREQUAL "Vulkan")
        set(mealkit_editor_build VulkanEditor)
    else()
        set(mealkit_editor_build Editor)
    endif()
    set(MEALKIT_EDITOR_SDK "${MEALKIT_ENGINE_DIR}/build/${mealkit_editor_build}" CACHE PATH "엔진 에디터 빌드 폴더 (mealkit.dll, MealKitEditor.exe)")
    if(NOT EXISTS "${MEALKIT_EDITOR_SDK}/MealKitTargets.cmake")
        message(FATAL_ERROR "Engine editor build not found in ${MEALKIT_EDITOR_SDK}. Run ${MEALKIT_ENGINE_DIR}\\build.bat ${mealkit_editor_build} first.")
    endif()
    include("${MEALKIT_EDITOR_SDK}/MealKitTargets.cmake")
    set(MEALKIT_STEAM_DLL "")
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
    # LTO는 엔진처럼 MSVC만. Clang의 LTO 라이브러리는 LLVM 비트코드라 Linux의 GNU ld가 읽지 못한다.
    if(NOT MSVC)
        set(INTERPROCEDURAL_OPTIMIZATION OFF CACHE BOOL "" FORCE)
    endif()
    set(USE_SSE4_1 ON CACHE BOOL "" FORCE)
    set(USE_SSE4_2 ON CACHE BOOL "" FORCE)
    # Jolt는 기본으로 정적 CRT를 강제한다. 에디터 빌드에서는 mealkit.dll과 같은 동적 CRT로 맞춘다.
    if(MEALKIT_BUILD_KIND STREQUAL "Editor")
        set(USE_STATIC_MSVC_RUNTIME_LIBRARY OFF CACHE BOOL "" FORCE)
    else()
        set(USE_STATIC_MSVC_RUNTIME_LIBRARY ON CACHE BOOL "" FORCE)
    endif()
    add_subdirectory(${MEALKIT_ENGINE_DIR}/third_party/JoltPhysics/Build ${CMAKE_BINARY_DIR}/third_party/jolt EXCLUDE_FROM_ALL)
    # Jolt의 Windows 분기는 clang-cl에 -ffp-contract=off를 주지 않는다 (MSVC는 /fp:precise로 받는다).
    if(MSVC AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        target_compile_options(Jolt PRIVATE /clang:-ffp-contract=off)
    endif()

    # Tracy: 마커 헤더는 항상 쓰고, 클라이언트는 프로파일 빌드에서만 링크한다.
    if(MEALKIT_PROFILE)
        add_library(mealkit_tracy STATIC ${MEALKIT_ENGINE_DIR}/third_party/tracy/public/TracyClient.cpp)
        # 프로파일러가 붙었을 때만 수집하고, 127.0.0.1에서만 접속을 받는다 (방화벽 창 방지)
        target_compile_definitions(mealkit_tracy PUBLIC TRACY_ENABLE TRACY_ON_DEMAND TRACY_ONLY_LOCALHOST)
        if(WIN32)
            target_compile_options(mealkit_tracy PRIVATE /W0)
            target_link_libraries(mealkit_tracy PUBLIC ws2_32 dbghelp)
        endif()
    else()
        add_library(mealkit_tracy INTERFACE)
    endif()
    target_include_directories(mealkit_tracy SYSTEM INTERFACE ${MEALKIT_ENGINE_DIR}/third_party/tracy/public)

    add_subdirectory(${MEALKIT_ENGINE_DIR}/engine ${CMAKE_BINARY_DIR}/engine)
endif()

# ---------------------------------------------------------------- 게임

# mealkit_add_game(<이름>
#     [SOURCE_DIR <폴더>]             게임 코드 폴더 (기본 source). 그 아래 sim/ view/ app/ 의 .h/.cpp를 모두 쓴다.
#                                     파일을 추가해도 CMakeLists.txt를 고치지 않는다 (CONFIGURE_DEPENDS GLOB). include 기준 폴더도 이것이다.
#     [GAME_MAIN <main.cpp>]          출시용 게임 실행 파일 <이름>.exe (Game 빌드). 기본 <SOURCE_DIR>/app/main.cpp
#     [MODULE <GameModule.cpp>]       MEALKIT_GAME_MODULE이 있는 파일 → <이름>_game.dll (Editor 빌드). 기본 <SOURCE_DIR>/app/GameModule.cpp
#     [HEADLESS <Headless.cpp>]       MEALKIT_HEADLESS_GAME이 있는 파일 → <이름>_headless 실행 파일 (Game 빌드, sim/ + 이 파일만, 창·렌더러 없음).
#                                     기본 <SOURCE_DIR>/app/Headless.cpp. 없으면 헤드리스 타깃을 만들지 않는다 (Linux·macOS에서는 필수)
#     [SOURCES <게임 코드...>]         예전 방식: 파일을 직접 나열한다 (주면 GLOB 대신 이것을 쓴다)
#     [STEAM_APP_ID <id>])            기본 480 (Valve 테스트 앱 Spacewar)
#
# 경로 안에 sim/ 폴더가 있는 파일은 빌드 전에 SimCheck(engine/tools/SimCheck.cpp)가 결정론 규칙을 검사하고, 어기면 빌드가 실패한다.
# 프로젝트 third_party/<lib>/ (mealkit.txt가 있는 폴더): CMakeLists.txt가 있으면 add_subdirectory 후 mealkit.txt의 cmakeTarget을,
# 없으면 헤더 전용으로 include 폴더(mealkit.txt의 include, 기본 그 폴더)를 게임 exe·DLL에만 붙인다. sim/에서는 쓸 수 없다(SimCheck SIM011).
# 프로젝트 폴더는 최상위 CMakeLists.txt가 있는 곳이다. 그 아래 assets/를 쓰고, 출시 빌드는 export/<이름>/에 만든다.
# game_export 타깃: 게임 실행 파일 + assets + Steam DLL + 라이선스 고지를 export/<이름>/에 모은다 (build.bat Export).
function(mealkit_add_game name)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "" "SOURCE_DIR;GAME_MAIN;MODULE;HEADLESS;STEAM_APP_ID" "SOURCES")
    if(NOT ARG_SOURCE_DIR)
        set(ARG_SOURCE_DIR source)
    endif()
    get_filename_component(source_dir "${ARG_SOURCE_DIR}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    if(NOT ARG_GAME_MAIN)
        set(ARG_GAME_MAIN "${source_dir}/app/main.cpp")
    endif()
    if(NOT ARG_MODULE)
        set(ARG_MODULE "${source_dir}/app/GameModule.cpp")
    endif()
    if(NOT ARG_HEADLESS)
        set(ARG_HEADLESS "${source_dir}/app/Headless.cpp")
    endif()
    get_filename_component(ARG_HEADLESS "${ARG_HEADLESS}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    get_filename_component(ARG_GAME_MAIN "${ARG_GAME_MAIN}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    get_filename_component(ARG_MODULE "${ARG_MODULE}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    if(NOT ARG_STEAM_APP_ID)
        set(ARG_STEAM_APP_ID 480)
    endif()
    set(project_dir ${CMAKE_SOURCE_DIR})

    # 게임 라이브러리 (docs/CODING_MODEL.md "외부 라이브러리 두기"). 엔진 저장소의 third_party/는 엔진 라이브러리라 여기서 보지 않는다.
    set(game_libs "")
    set(game_lib_includes "")
    get_filename_component(project_dir_abs "${project_dir}" ABSOLUTE)
    if(NOT project_dir_abs STREQUAL MEALKIT_ENGINE_DIR)
        file(GLOB lib_metas CONFIGURE_DEPENDS "${project_dir}/third_party/*/mealkit.txt")
        foreach(meta IN LISTS lib_metas)
            get_filename_component(lib_dir "${meta}" DIRECTORY)
            get_filename_component(lib_name "${lib_dir}" NAME)
            file(STRINGS "${meta}" lib_lines ENCODING UTF-8)
            set(lib_target "")
            set(lib_include "")
            foreach(line IN LISTS lib_lines)
                if(line MATCHES "^[ \t]*cmakeTarget[ \t]+(.+)$")
                    string(STRIP "${CMAKE_MATCH_1}" lib_target)
                elseif(line MATCHES "^[ \t]*include[ \t]+(.+)$")
                    string(STRIP "${CMAKE_MATCH_1}" lib_include)
                endif()
            endforeach()
            if(EXISTS "${lib_dir}/CMakeLists.txt")
                if(NOT lib_target)
                    message(FATAL_ERROR "${meta}: the library has a CMakeLists.txt - name the target to link with 'cmakeTarget <target>'")
                endif()
                add_subdirectory("${lib_dir}" "${CMAKE_BINARY_DIR}/third_party/game_${lib_name}" EXCLUDE_FROM_ALL)
                list(APPEND game_libs ${lib_target})
            endif()
            if(lib_include)
                list(APPEND game_lib_includes "${lib_dir}/${lib_include}")
            else()
                list(APPEND game_lib_includes "${lib_dir}")
            endif()
        endforeach()
    endif()
    set(simcheck_third_party "")
    foreach(dir IN LISTS game_lib_includes)
        list(APPEND simcheck_third_party --third-party "${dir}")
    endforeach()

    if(ARG_SOURCES)
        set(game_sources "")
        foreach(f IN LISTS ARG_SOURCES)
            get_filename_component(f "${f}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
            list(APPEND game_sources "${f}")
        endforeach()
    else()
        file(GLOB_RECURSE game_sources CONFIGURE_DEPENDS
            "${source_dir}/sim/*.h" "${source_dir}/sim/*.cpp"
            "${source_dir}/view/*.h" "${source_dir}/view/*.cpp"
            "${source_dir}/app/*.h" "${source_dir}/app/*.cpp")
        if(NOT game_sources)
            message(FATAL_ERROR "mealkit_add_game(${name}): no game code in ${source_dir}/sim, view, app")
        endif()
    endif()
    list(REMOVE_ITEM game_sources "${ARG_GAME_MAIN}" "${ARG_MODULE}")
    foreach(f "${ARG_GAME_MAIN}" "${ARG_MODULE}")
        if(NOT EXISTS "${f}")
            message(FATAL_ERROR "mealkit_add_game(${name}): ${f} not found")
        endif()
    endforeach()

    # sim/ 결정론 검사: 통과하면 stamp를 남기고, sim/ 파일이 바뀔 때만 다시 돈다
    set(sim_sources "")
    foreach(f IN LISTS game_sources)
        file(RELATIVE_PATH rel "${source_dir}" "${f}")
        if(rel MATCHES "(^|/)sim/")
            list(APPEND sim_sources "${f}")
        endif()
    endforeach()
    if(sim_sources)
        set(sim_stamp "${CMAKE_CURRENT_BINARY_DIR}/${name}_simcheck.stamp")
        add_custom_command(OUTPUT "${sim_stamp}"
            COMMAND mealkit_simcheck "${sim_stamp}" ${simcheck_third_party} ${sim_sources}
            DEPENDS mealkit_simcheck ${sim_sources}
            COMMENT "SimCheck ${name}: sim/ determinism rules"
            VERBATIM)
        add_custom_target(${name}_simcheck DEPENDS "${sim_stamp}")
    endif()

    # 빌드 정보 (cmake/BuildInfo.cmake, 빌드마다): 게임 버전(mealkit.project version), git 커밋, 시뮬레이션 버전 → mealkit_build_info.h.
    # 시뮬레이션 버전은 시뮬레이션에 닿는 소스(게임 sim/ + 헤드리스 파일 + 엔진 코어 폴더 + Jolt 버전)의 해시다:
    # 실행 파일이 달라도(다른 OS·컴파일러) 같은 소스면 같은 값이라, 접속할 때 실행 파일 대신 이것을 비교한다 (mealkit/game/BuildInfo.h)
    set(build_info_dir "${CMAKE_CURRENT_BINARY_DIR}/${name}_build_info")
    set(engine_src "${MEALKIT_ENGINE_DIR}/engine/src/mealkit")
    file(GLOB_RECURSE engine_sim_sources CONFIGURE_DEPENDS
        "${engine_src}/core/*.h" "${engine_src}/core/*.cpp" "${engine_src}/net/*.h" "${engine_src}/net/*.cpp"
        "${engine_src}/physics/*.h" "${engine_src}/physics/*.cpp" "${engine_src}/sim/*.h"
        "${engine_src}/data/*.h" "${engine_src}/data/*.cpp" "${engine_src}/scene/*.h" "${engine_src}/scene/*.cpp"
        "${engine_src}/asset/*.h" "${engine_src}/asset/*.cpp")
    list(SORT engine_sim_sources)
    list(APPEND engine_sim_sources "${engine_src}/game/Simulation.h" "${engine_src}/game/GameData.h" "${engine_src}/game/GameData.cpp"
        "${engine_src}/game/Loadout.h" "${engine_src}/game/InputRecord.h" "${engine_src}/game/InputRecord.cpp"
        "${MEALKIT_ENGINE_DIR}/third_party/JoltPhysics/mealkit.txt")
    set(build_info_sources ${sim_sources})
    list(SORT build_info_sources)
    list(APPEND build_info_sources "${ARG_HEADLESS}" ${engine_sim_sources})
    string(REPLACE ";" "\n" build_info_list "${build_info_sources}")
    file(CONFIGURE OUTPUT "${build_info_dir}/sim_sources.txt" CONTENT "${build_info_list}\n" @ONLY)
    add_custom_target(${name}_buildinfo
        COMMAND ${CMAKE_COMMAND} "-DOUT=${build_info_dir}/mealkit_build_info.h" "-DLIST=${build_info_dir}/sim_sources.txt"
            "-DPROJECT_FILE=${project_dir}/mealkit.project" "-DPROJECT_DIR=${project_dir_abs}" "-DENGINE_DIR=${MEALKIT_ENGINE_DIR}"
            -P "${MEALKIT_ENGINE_DIR}/cmake/BuildInfo.cmake"
        BYPRODUCTS "${build_info_dir}/mealkit_build_info.h"
        COMMENT "Build info ${name}: version, commit, simulation version"
        VERBATIM)

    # 문자열 표(assets/strings/*.txt)의 글자가 assets/fonts의 글꼴에 다 있는지 (창 있는 빌드, mealkit_stringcheck)
    set(string_check "")
    if(NOT MEALKIT_HEADLESS_ONLY)
        file(GLOB string_files CONFIGURE_DEPENDS "${project_dir}/assets/strings/*.txt")
        file(GLOB_RECURSE font_files CONFIGURE_DEPENDS "${project_dir}/assets/fonts/*.ttf" "${project_dir}/assets/fonts/*.otf"
             "${project_dir}/assets/fonts/*.ttc")
        if(string_files)
            set(string_stamp "${CMAKE_CURRENT_BINARY_DIR}/${name}_stringcheck.stamp")
            add_custom_command(OUTPUT "${string_stamp}"
                COMMAND mealkit_stringcheck "${string_stamp}" --fonts "${project_dir}/assets/fonts" ${string_files}
                DEPENDS mealkit_stringcheck ${string_files} ${font_files}
                COMMENT "StringCheck ${name}: characters of assets/strings are in assets/fonts"
                VERBATIM)
            add_custom_target(${name}_stringcheck DEPENDS "${string_stamp}")
            set(string_check ${name}_stringcheck)
        endif()
    endif()

    # 헤드리스 (docs/CODING_MODEL.md "창 없이(헤드리스·CI) 게임을 돌리는 방법"): sim/ + Headless.cpp만, 엔진은 코어(mealkit_core)만 링크한다. 창·렌더러·오디오 없음.
    # CI가 Linux·macOS에서도 이것을 빌드해 틱별 해시를 비교한다. Headless.cpp는 출시 exe에도 들어간다(--headless, GameSession이 같은 함수를 쓴다).
    if(NOT MEALKIT_BUILD_KIND STREQUAL "Editor")
        if(EXISTS "${ARG_HEADLESS}")
            add_executable(${name}_headless "${ARG_HEADLESS}" ${sim_sources})
            target_link_libraries(${name}_headless PRIVATE mealkit_core)
            target_include_directories(${name}_headless PRIVATE ${source_dir})
            target_compile_definitions(${name}_headless PRIVATE MEALKIT_GAME_NAME="${name}" MEALKIT_HEADLESS_TARGET=1)
            target_include_directories(${name}_headless PRIVATE "${build_info_dir}")
            add_dependencies(${name}_headless ${name}_buildinfo)
            if(sim_sources)
                add_dependencies(${name}_headless ${name}_simcheck)
            endif()
        elseif(MEALKIT_HEADLESS_ONLY)
            message(FATAL_ERROR "mealkit_add_game(${name}): ${ARG_HEADLESS} not found (needed for headless builds)")
        endif()
        if(MEALKIT_HEADLESS_ONLY)
            return()
        endif()
    endif()

    if(MEALKIT_BUILD_KIND STREQUAL "Editor")
        # 에디터가 불러오는 게임 코드. 엔진은 mealkit.dll에서 가져온다.
        add_library(${name}_game SHARED ${ARG_MODULE} ${game_sources})
        target_link_libraries(${name}_game PRIVATE mealkit ${game_libs})
        target_include_directories(${name}_game PRIVATE ${source_dir})
        target_include_directories(${name}_game SYSTEM PRIVATE ${game_lib_includes})
        target_compile_definitions(${name}_game PRIVATE MEALKIT_GAME_NAME="${name}")
        target_include_directories(${name}_game PRIVATE "${build_info_dir}")
        add_dependencies(${name}_game ${name}_buildinfo)
        if(sim_sources)
            add_dependencies(${name}_game ${name}_simcheck)
        endif()
        if(string_check)
            add_dependencies(${name}_game ${string_check})
        endif()
        return()
    endif()

    # 실행 파일 아이콘: mealkit.project의 icon(빈 값이면 icon.png, 프로젝트 폴더 기준) → 여러 크기 .ico → 아이콘 리소스 1번.
    # Window가 그것을 창 아이콘으로 쓴다. 그림이 없으면 엔진 기본 아이콘(밀키트)을 넣는다.
    set(icon_image "icon.png")
    set(project_file "${project_dir}/mealkit.project")
    if(EXISTS "${project_file}")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${project_file}")
        file(STRINGS "${project_file}" icon_lines REGEX "^[ \t]*icon([ \t=]|$)" ENCODING UTF-8)
        foreach(line IN LISTS icon_lines)
            string(REGEX REPLACE "^[ \t]*icon[ \t]*=?[ \t]*" "" line "${line}")
            string(STRIP "${line}" line)
            if(line)
                set(icon_image "${line}")
            endif()
        endforeach()
    endif()
    file(TO_CMAKE_PATH "${icon_image}" icon_image)
    get_filename_component(icon_image "${icon_image}" ABSOLUTE BASE_DIR "${project_dir}")
    # 그림이 나중에 생기거나 지워져도 다시 구성되도록 GLOB으로 지켜본다
    file(GLOB icon_found CONFIGURE_DEPENDS "${icon_image}")
    set(icon_rc "${CMAKE_CURRENT_BINARY_DIR}/${name}_icon.rc")
    if(icon_found)
        set(icon_file "${CMAKE_CURRENT_BINARY_DIR}/${name}.ico")
        add_custom_command(OUTPUT "${icon_file}"
            COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File "${MEALKIT_ENGINE_DIR}/cmake/MakeIcon.ps1"
                -Image "${icon_image}" -Out "${icon_file}"
            DEPENDS "${icon_image}" "${MEALKIT_ENGINE_DIR}/cmake/MakeIcon.ps1"
            COMMENT "Game icon ${name}: ${icon_image}"
            VERBATIM)
    else()
        set(icon_file "${MEALKIT_ENGINE_DIR}/engine/res/MealKitEditor.ico")
    endif()
    file(CONFIGURE OUTPUT "${icon_rc}" CONTENT "#pragma code_page(65001)\n1 ICON \"${icon_file}\"\n" @ONLY)
    set_source_files_properties("${icon_rc}" PROPERTIES OBJECT_DEPENDS "${icon_file}")

    # 게임 exe: 에디터는 들어가지 않는다. ImGui(디버그 UI)는 MEALKIT_DEBUG_UI일 때만 (Export는 꺼서 뺀다).
    add_executable(${name} ${ARG_GAME_MAIN} ${game_sources} "${icon_rc}")
    if(icon_found)
        target_sources(${name} PRIVATE "${icon_file}")
    endif()
    target_link_libraries(${name} PRIVATE mealkit ${game_libs})
    target_include_directories(${name} PRIVATE ${source_dir})
    target_include_directories(${name} SYSTEM PRIVATE ${game_lib_includes})
    target_compile_definitions(${name} PRIVATE MEALKIT_GAME_NAME="${name}")
    target_include_directories(${name} PRIVATE "${build_info_dir}")
    add_dependencies(${name} ${name}_buildinfo)
    if(sim_sources)
        add_dependencies(${name} ${name}_simcheck)
    endif()
    if(string_check)
        add_dependencies(${name} ${string_check})
    endif()
    if(MEALKIT_STEAM_DLL)
        add_custom_command(TARGET ${name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${MEALKIT_STEAM_DLL} $<TARGET_FILE_DIR:${name}>)
        # 개발 중 실행용. 출시할 때는 내 앱 ID로 바꾸고, 480이 아니면 출시 폴더에 넣지 않는다.
        file(WRITE ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/steam_appid.txt "${ARG_STEAM_APP_ID}")
    endif()

    # 출시 전 검사: 폴더 구조(tools/check_layout.ps1, 어기면 실패) → 고지문 합치기(MealKit + 게임 exe의 엔진 라이브러리 + 게임 라이브러리 + 글꼴)
    # → 출시 폴더 (MealKitExport.cmake가 exe 크기 예산도 본다)
    set(export_notices "${CMAKE_BINARY_DIR}/export_notices/THIRD_PARTY_NOTICES.txt")
    # 출시 폴더: export/<이름>, mealkit.project의 exportVersionFolder가 켜져 있으면 export/<이름>-<version>
    set(export_dir "${project_dir}/export/${name}")
    if(EXISTS "${project_file}")
        file(STRINGS "${project_file}" export_lines REGEX "^[ \t]*(version|exportVersionFolder)([ \t=]|$)" ENCODING UTF-8)
        set(export_version "")
        set(export_with_version FALSE)
        foreach(line IN LISTS export_lines)
            if(line MATCHES "^[ \t]*version[ \t]*=?[ \t]*(.*)$")
                string(STRIP "${CMAKE_MATCH_1}" export_version)
            elseif(line MATCHES "^[ \t]*exportVersionFolder[ \t]*=?[ \t]*(true|1)")
                set(export_with_version TRUE)
            endif()
        endforeach()
        if(export_with_version AND export_version)
            set(export_dir "${project_dir}/export/${name}-${export_version}")
        endif()
    endif()
    add_custom_target(game_export
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File "${MEALKIT_ENGINE_DIR}/tools/check_layout.ps1"
            -Project "${project_dir}"
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File "${MEALKIT_ENGINE_DIR}/tools/gen_notices.ps1"
            -Project "${project_dir}" -Out "${export_notices}"
        COMMAND ${CMAKE_COMMAND}
            -DGAME_EXE=$<TARGET_FILE:${name}>
            -DASSETS_DIR=${project_dir}/assets
            -DPROJECT_FILE=${project_dir}/mealkit.project
            -DOUT_DIR=${export_dir}
            -DSTEAM_DLL=${MEALKIT_STEAM_DLL}
            -DSTEAM_APP_ID=${ARG_STEAM_APP_ID}
            -DNOTICES=${export_notices}
            -DEXE_BUDGET_MB=5
            -P ${MEALKIT_ENGINE_DIR}/cmake/MealKitExport.cmake
        DEPENDS ${name}
        COMMENT "Exporting ${name}"
        VERBATIM)
endfunction()

# Attaches the MealKit engine to a CMake project. Shared by the engine repo (D:\MealKit) and game project folders.
#
#   cmake_minimum_required(VERSION 3.25)
#   project(MyGame LANGUAGES CXX)
#   include(${MEALKIT_ROOT}/cmake/MealKit.cmake)
#   mealkit_add_game(MyGame)   # source/sim, source/view, source/app
#
# Build kind (MEALKIT_BUILD_KIND, set by build_project.bat):
#   Game    shipping: a single <name>.exe with the engine as a static library (static CRT). build\Release, build\Debug, build\Profile
#   Editor  for the editor: engine is mealkit.dll (dynamic CRT), game code is <name>_game.dll. build\Editor
#           The engine repo builds mealkit.dll and MealKitEditor.exe; game projects import them (MealKitTargets.cmake) and build only the game DLL.
#
# Must be included from the top-level CMakeLists.txt: the global settings below (C++20, CRT) must apply to game targets too for linking.
include_guard(GLOBAL)

# Windows uses MSVC (or clang-cl). Linux/macOS build only headless targets (<game>_headless, engine unit tests) with Clang/GCC (CI determinism comparison; full port is ROADMAP stage 5).
if(WIN32)
    if(NOT MSVC)
        message(FATAL_ERROR "MealKit on Windows needs MSVC or clang-cl.")
    endif()
    # build.ps1 HeadlessOnly: the Linux/macOS CI build (core only) on Windows, to catch front-end leaks before pushing
    option(MEALKIT_FORCE_HEADLESS_ONLY "Build only the core and headless targets, as on Linux/macOS" OFF)
    set(MEALKIT_HEADLESS_ONLY ${MEALKIT_FORCE_HEADLESS_ONLY})
else()
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        message(FATAL_ERROR "MealKit headless builds need Clang or GCC (got ${CMAKE_CXX_COMPILER_ID}).")
    endif()
    set(MEALKIT_HEADLESS_ONLY ON)
endif()

get_filename_component(MEALKIT_ENGINE_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
# Engine version (repo root VERSION). Compared with the project's mealkit.project engineVersion.
file(STRINGS "${MEALKIT_ENGINE_DIR}/VERSION" MEALKIT_VERSION LIMIT_COUNT 1)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${MEALKIT_ENGINE_DIR}/VERSION")
# Docs address that engine messages point to (docs repo MealKit-docs, not yet open before release).
# Before 1.0 the docs have one branch, master, so latest; from 1.0, the docs branch of the engine MINOR version (docs/DECISIONS.md 2026-10-21)
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

set(MEALKIT_BUILD_KIND "Game" CACHE STRING "Game: shipping static exe / Editor: mealkit.dll + MealKitEditor + game DLL")
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
    # Dynamic CRT so the editor and game DLL share a heap with the engine (mealkit.dll). Link-time optimization is off (faster game DLL rebuilds).
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    # Linux/macOS: the static libraries linked into libmealkit.so/.dylib (Jolt, ImGui, mkcontract ...) must be position independent
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
else()
    # Static CRT: runs without a redistributable, and every PC runs the same CRT code, which also helps determinism.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    # The clang-cl build (build.bat ClangCL) is for determinism comparison, so no link-time optimization (it would need lld-link).
    if(MSVC AND NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
    endif()
endif()

# Graphics API backend (mealkit/rhi/<api>/, one per build). Shipping Windows builds use DX12; Vulkan via build.bat Vulkan/VulkanEditor.
set(MEALKIT_RHI "DX12" CACHE STRING "RHI backend: DX12 / Vulkan")
set_property(CACHE MEALKIT_RHI PROPERTY STRINGS DX12 Vulkan)
if(NOT MEALKIT_RHI MATCHES "^(DX12|Vulkan)$")
    message(FATAL_ERROR "MEALKIT_RHI must be DX12 or Vulkan (got '${MEALKIT_RHI}')")
endif()

option(MEALKIT_PROFILE "Enable Tracy profiling markers (build.bat Profile)" OFF)
# Debug UI (Dear ImGui): included in dev builds (build\Release/Debug/Profile) and the editor, excluded from shipping builds (build.bat Export → build\Export).
# When on, the engine target passes MEALKIT_DEBUG_UI=1 and the imgui.h path to the game. Game code wraps it in #if MEALKIT_DEBUG_UI.
option(MEALKIT_DEBUG_UI "Include the debug UI (Dear ImGui) (OFF for build.bat Export)" ON)
if(MEALKIT_HEADLESS_ONLY)
    set(MEALKIT_DEBUG_UI OFF)
elseif(MEALKIT_BUILD_KIND STREQUAL "Editor")
    set(MEALKIT_DEBUG_UI ON) # the editor draws with ImGui
endif()
# Live connection (mealkit/live/Server.h, the contract between tools): dev build game exes open a 127.0.0.1 server. Excluded from shipping builds (Export).
# The editor opens it regardless of this value. When on, the engine target passes MEALKIT_LIVE=1
option(MEALKIT_LIVE "Include the live connection server (OFF for build.bat Export)" ON)
if(MEALKIT_HEADLESS_ONLY)
    set(MEALKIT_LIVE OFF)
endif()

# Engine presets (docs/ENGINE_MODULES.md "Presets and the project's module list"): mealkit.project "enginePreset <name>" (standard if absent)
# plus "engineModules <names>" added to it. standard is every module; the others list the optional modules they leave out.
set(MEALKIT_PRESETS standard solo)
set(MEALKIT_PRESET_standard_OFF "")
set(MEALKIT_PRESET_solo_OFF net online)
# out = the variant name for this project file ("standard" = the base libraries)
function(mealkit_project_preset project_file out)
    set(preset standard)
    set(added "")
    if(EXISTS "${project_file}")
        file(STRINGS "${project_file}" lines REGEX "^[ \t]*engine(Preset|Modules)[ \t]" ENCODING UTF-8)
        foreach(line IN LISTS lines)
            if(line MATCHES "^[ \t]*enginePreset[ \t]+([a-z0-9-]+)")
                set(preset ${CMAKE_MATCH_1})
            elseif(line MATCHES "^[ \t]*engineModules[ \t]+(.+)$")
                string(REGEX REPLACE "[ \t]+" ";" words "${CMAKE_MATCH_1}")
                list(APPEND added ${words})
            endif()
        endforeach()
    endif()
    if(NOT preset IN_LIST MEALKIT_PRESETS)
        message(FATAL_ERROR "${project_file}: enginePreset '${preset}' is not one of ${MEALKIT_PRESETS}")
    endif()
    set(off ${MEALKIT_PRESET_${preset}_OFF})
    if(added)
        list(REMOVE_ITEM off ${added})
        if(off AND NOT off STREQUAL MEALKIT_PRESET_${preset}_OFF)
            message(FATAL_ERROR "${project_file}: adding only some of '${MEALKIT_PRESET_${preset}_OFF}' to preset ${preset} isn't a built combination yet (add all or pick another preset)")
        endif()
    endif()
    if(off)
        set(${out} ${preset} PARENT_SCOPE)
    else()
        set(${out} standard PARENT_SCOPE)
    endif()
endfunction()

# Variants the engine builds: those the projects in this build use (engine repo: samples/*, plus MEALKIT_EXTRA_PRESETS for CI)
set(MEALKIT_EXTRA_PRESETS "" CACHE STRING "Extra engine presets to build (solo); samples built with them get a _<preset> suffix")
set(MEALKIT_PRESETS_USED "")
if(MEALKIT_IS_ENGINE)
    file(GLOB mealkit_project_files "${MEALKIT_ENGINE_DIR}/samples/*/mealkit.project")
else()
    set(mealkit_project_files "${CMAKE_SOURCE_DIR}/mealkit.project")
endif()
foreach(f ${mealkit_project_files})
    mealkit_project_preset("${f}" used)
    if(NOT used STREQUAL "standard")
        list(APPEND MEALKIT_PRESETS_USED ${used})
    endif()
endforeach()
foreach(extra ${MEALKIT_EXTRA_PRESETS})
    if(NOT extra STREQUAL "standard")
        list(APPEND MEALKIT_PRESETS_USED ${extra})
    endif()
endforeach()
list(REMOVE_DUPLICATES MEALKIT_PRESETS_USED)

if(MEALKIT_BUILD_KIND STREQUAL "Editor" AND NOT MEALKIT_IS_ENGINE)
    # ---------------------------------------------------------------- Editor build of a game project: imports the engine
    # The Vulkan editor (build.bat VulkanEditor) uses the engine's build\VulkanEditor
    if(MEALKIT_RHI STREQUAL "Vulkan")
        set(mealkit_editor_build VulkanEditor)
    else()
        set(mealkit_editor_build Editor)
    endif()
    set(MEALKIT_EDITOR_SDK "${MEALKIT_ENGINE_DIR}/build/${mealkit_editor_build}" CACHE PATH "Engine editor build folder (mealkit.dll, MealKitEditor.exe)")
    if(NOT EXISTS "${MEALKIT_EDITOR_SDK}/MealKitTargets.cmake")
        message(FATAL_ERROR "Engine editor build not found in ${MEALKIT_EDITOR_SDK}. Run ${MEALKIT_ENGINE_DIR}\\build.bat ${mealkit_editor_build} first.")
    endif()
    include("${MEALKIT_EDITOR_SDK}/MealKitTargets.cmake")
    set(MEALKIT_STEAM_DLL "")
    set(MEALKIT_EOS_DLL "")
else()
    # ---------------------------------------------------------------- Build the engine from source

    # Jolt Physics: deterministic build. Uses up to SSE4.2 only so it runs on office CPUs too (including Pentium/Celeron).
    # The same executable runs the same instructions on any CPU, so determinism holds.
    foreach(jolt_off
            TARGET_UNIT_TESTS TARGET_HELLO_WORLD TARGET_PERFORMANCE_TEST TARGET_SAMPLES TARGET_VIEWER
            ENABLE_INSTALL ENABLE_ALL_WARNINGS OVERRIDE_CXX_FLAGS GENERATE_DEBUG_SYMBOLS
            USE_AVX USE_AVX2 USE_AVX512 USE_LZCNT USE_TZCNT USE_F16C USE_FMADD
            DEBUG_RENDERER_IN_DEBUG_AND_RELEASE PROFILER_IN_DEBUG_AND_RELEASE ENABLE_OBJECT_STREAM
            FLOATING_POINT_EXCEPTIONS_ENABLED JPH_USE_DX12 JPH_USE_VK JPH_USE_MTL JPH_USE_CPU_COMPUTE)
        set(${jolt_off} OFF CACHE BOOL "" FORCE)
    endforeach()
    set(CROSS_PLATFORM_DETERMINISTIC ON CACHE BOOL "" FORCE)
    # LTO only for MSVC, like the engine. Clang's LTO libraries are LLVM bitcode that Linux GNU ld can't read.
    if(NOT MSVC)
        set(INTERPROCEDURAL_OPTIMIZATION OFF CACHE BOOL "" FORCE)
    endif()
    set(USE_SSE4_1 ON CACHE BOOL "" FORCE)
    set(USE_SSE4_2 ON CACHE BOOL "" FORCE)
    # Jolt forces a static CRT by default. In the editor build, match mealkit.dll's dynamic CRT.
    if(MEALKIT_BUILD_KIND STREQUAL "Editor")
        set(USE_STATIC_MSVC_RUNTIME_LIBRARY OFF CACHE BOOL "" FORCE)
    else()
        set(USE_STATIC_MSVC_RUNTIME_LIBRARY ON CACHE BOOL "" FORCE)
    endif()
    add_subdirectory(${MEALKIT_ENGINE_DIR}/third_party/JoltPhysics/Build ${CMAKE_BINARY_DIR}/third_party/jolt EXCLUDE_FROM_ALL)
    # Jolt's Windows branch doesn't pass -ffp-contract=off to clang-cl (MSVC gets it via /fp:precise).
    if(MSVC AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        target_compile_options(Jolt PRIVATE /clang:-ffp-contract=off)
    endif()

    # Tracy: marker headers are always used; the client is linked only in profile builds.
    if(MEALKIT_PROFILE)
        add_library(mealkit_tracy STATIC ${MEALKIT_ENGINE_DIR}/third_party/tracy/public/TracyClient.cpp)
        # Collect only when a profiler is attached, and accept connections only on 127.0.0.1 (avoids the firewall prompt)
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

# ---------------------------------------------------------------- Game

# mealkit_add_game(<name>
#     [SOURCE_DIR <folder>]           game code folder (default source). Uses all .h/.cpp under its sim/ view/ app/.
#                                     Adding files needs no CMakeLists.txt edits (CONFIGURE_DEPENDS GLOB). This is also the include base folder.
#     [GAME_MAIN <main.cpp>]          shipping game executable <name>.exe (Game build). Default <SOURCE_DIR>/app/main.cpp
#     [MODULE <GameModule.cpp>]       file with MEALKIT_GAME_MODULE → <name>_game.dll (Editor build). Default <SOURCE_DIR>/app/GameModule.cpp
#     [HEADLESS <Headless.cpp>]       file with MEALKIT_HEADLESS_GAME → <name>_headless executable (Game build, sim/ + this file only, no window/renderer).
#                                     Default <SOURCE_DIR>/app/Headless.cpp. If missing, no headless target is made (required on Linux/macOS)
#     [SOURCES <game code...>]        old style: list files directly (if given, used instead of GLOB)
#     [STEAM_APP_ID <id>])            default 480 (Valve test app Spacewar)
#
# Files with a sim/ folder in their path are checked for determinism rules by SimCheck (engine/tools/SimCheck.cpp) before the build; violations fail the build.
# Project third_party/<lib>/ (folders with mealkit.txt): if it has a CMakeLists.txt, add_subdirectory then link mealkit.txt's cmakeTarget;
# otherwise header-only: the include folder (mealkit.txt's include, default that folder) is attached only to the game exe/DLL. Not usable in sim/ (SimCheck SIM011).
# The project folder is where the top-level CMakeLists.txt is. assets/ under it is used, and shipping builds go to export/<name>/.
# game_export target: gathers game executable + assets + Steam DLL + license notices into export/<name>/ (build.bat Export).
function(mealkit_add_game name)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "" "SOURCE_DIR;GAME_MAIN;MODULE;HEADLESS;STEAM_APP_ID;PRESET" "SOURCES")
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
    # Project folder = the folder whose CMakeLists.txt calls this (a game project's root, or samples/<name> in the engine repo)
    set(project_dir ${CMAKE_CURRENT_SOURCE_DIR})
    # Engine preset: mealkit.project enginePreset (PRESET overrides, for preset checks of the samples). The editor's mealkit.dll has every module
    if(ARG_PRESET)
        set(engine_preset ${ARG_PRESET})
    else()
        mealkit_project_preset("${project_dir}/mealkit.project" engine_preset)
    endif()
    set(engine_suffix "")
    if(NOT engine_preset STREQUAL "standard")
        set(engine_suffix _${engine_preset})
    endif()

    # Game libraries (docs/CODING_MODEL.md "Placing external libraries"). The engine repo's third_party/ holds engine libraries, so it isn't scanned here.
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

    # sim/ determinism check: leaves a stamp when it passes, reruns only when sim/ files change
    set(sim_sources "")
    foreach(f IN LISTS game_sources)
        file(RELATIVE_PATH rel "${source_dir}" "${f}")
        if(rel MATCHES "(^|/)sim/")
            list(APPEND sim_sources "${f}")
        endif()
    endforeach()

    # Source modules (docs/MODULES.md): for each folder with modules/<name>/module.txt, gather sim/ and view/ like game code.
    # The installed list is the folder itself. Include path is modules/ → #include "cooldown/sim/Cooldown.h".
    # Module sim/ goes through SimCheck (--modules: SIM013 if it includes game source/, SIM014 for modules not in requires) and into the simulation version.
    set(modules_dir "${project_dir}/modules")
    set(module_includes "")
    set(simcheck_modules "")
    file(GLOB module_metas CONFIGURE_DEPENDS "${modules_dir}/*/module.txt")
    set(module_names "")
    foreach(meta IN LISTS module_metas)
        get_filename_component(module_dir "${meta}" DIRECTORY)
        get_filename_component(module_folder "${module_dir}" NAME)
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${meta}")
        file(STRINGS "${meta}" module_lines ENCODING UTF-8)
        set(m_name "")
        set(m_version "0")
        set(m_engine "")
        set(m_requires "")
        foreach(line IN LISTS module_lines)
            if(line MATCHES "^[ \t]*(name|version|engine|requires)([ \t]+(.*))?$")
                string(STRIP "${CMAKE_MATCH_3}" value)
                if(CMAKE_MATCH_1 STREQUAL "name")
                    set(m_name "${value}")
                elseif(CMAKE_MATCH_1 STREQUAL "version")
                    set(m_version "${value}")
                elseif(CMAKE_MATCH_1 STREQUAL "engine")
                    set(m_engine "${value}")
                else()
                    string(REGEX REPLACE "[ \t]+" ";" m_requires "${value}")
                endif()
            endif()
        endforeach()
        if(NOT m_name STREQUAL module_folder)
            message(FATAL_ERROR "${meta}: 'name ${m_name}' must match the folder name '${module_folder}'")
        endif()
        if(NOT m_name MATCHES "^[a-z0-9_]+$" OR m_name MATCHES "^(sim|view|app|assets|mealkit|std)$")
            message(FATAL_ERROR "${meta}: module name '${m_name}' must be lowercase letters, digits and _ (and not sim, view, app, assets, mealkit, std)")
        endif()
        if(m_engine AND NOT m_engine VERSION_EQUAL MEALKIT_VERSION_MINOR)
            message(WARNING "Module ${m_name} ${m_version} is made for engine ${m_engine}, this engine is ${MEALKIT_VERSION}. Check its docs (modules/${m_name}/docs) when it does not build.")
        endif()
        list(APPEND module_names ${m_name})
        set(module_version_${m_name} "${m_version}")
        set(module_requires_${m_name} "${m_requires}")
    endforeach()
    # requires: "name [minVersion] ..." — a word starting with a digit is the previous module's minimum version
    foreach(m IN LISTS module_names)
        set(req "")
        foreach(word IN LISTS module_requires_${m})
            if(word MATCHES "^[0-9]")
                if(req AND module_version_${req} VERSION_LESS word)
                    message(FATAL_ERROR "Module ${m} needs ${req} ${word} or newer, modules/${req} is ${module_version_${req}}")
                endif()
            else()
                set(req "${word}")
                if(NOT req IN_LIST module_names)
                    message(FATAL_ERROR "Module ${m} needs the module '${req}': add it to modules/${req}")
                endif()
            endif()
        endforeach()
        file(GLOB_RECURSE m_sim CONFIGURE_DEPENDS "${modules_dir}/${m}/sim/*.h" "${modules_dir}/${m}/sim/*.cpp")
        file(GLOB_RECURSE m_view CONFIGURE_DEPENDS "${modules_dir}/${m}/view/*.h" "${modules_dir}/${m}/view/*.cpp")
        list(APPEND game_sources ${m_sim} ${m_view})
        list(APPEND sim_sources ${m_sim})
    endforeach()
    if(module_names)
        set(module_includes "${modules_dir}")
        set(simcheck_modules --modules "${modules_dir}")
        list(JOIN module_names ", " module_list)
        message(STATUS "${name}: modules ${module_list}")
    endif()

    if(sim_sources)
        set(sim_stamp "${CMAKE_CURRENT_BINARY_DIR}/${name}_simcheck.stamp")
        add_custom_command(OUTPUT "${sim_stamp}"
            COMMAND mealkit_simcheck "${sim_stamp}" ${simcheck_third_party} ${simcheck_modules} ${sim_sources}
            DEPENDS mealkit_simcheck ${sim_sources}
            COMMENT "SimCheck ${name}: sim/ determinism rules"
            VERBATIM)
        add_custom_target(${name}_simcheck DEPENDS "${sim_stamp}")
    endif()

    # Build info (cmake/BuildInfo.cmake, every build): game version (mealkit.project version), git commit, simulation version → mealkit_build_info.h.
    # The simulation version is a hash of sources that touch the simulation (game sim/ + headless file + engine core folders + Jolt version):
    # same source gives the same value even across executables (other OS/compiler), so this is compared on connect instead of the executable (mealkit/game/BuildInfo.h)
    set(build_info_dir "${CMAKE_CURRENT_BINARY_DIR}/${name}_build_info")
    # Engine side: the kernel (engine/core) and every engine module whose manifest says "kind sim" (docs/ENGINE_MODULES.md)
    file(GLOB_RECURSE engine_sim_sources CONFIGURE_DEPENDS "${MEALKIT_ENGINE_DIR}/engine/core/*.h" "${MEALKIT_ENGINE_DIR}/engine/core/*.cpp")
    file(GLOB engine_module_manifests CONFIGURE_DEPENDS "${MEALKIT_ENGINE_DIR}/modules/*/engine-module.txt")
    foreach(manifest ${engine_module_manifests})
        file(STRINGS ${manifest} module_kind REGEX "^kind ")
        if(module_kind STREQUAL "kind sim")
            get_filename_component(module_dir ${manifest} DIRECTORY)
            file(GLOB_RECURSE module_sim_sources CONFIGURE_DEPENDS "${module_dir}/*.h" "${module_dir}/*.cpp")
            list(APPEND engine_sim_sources ${module_sim_sources})
        endif()
    endforeach()
    list(SORT engine_sim_sources)
    set(engine_game "${MEALKIT_ENGINE_DIR}/modules/game")
    foreach(f include/mealkit/game/Simulation.h include/mealkit/game/GameData.h src/mealkit/game/GameData.cpp
              include/mealkit/game/Loadout.h include/mealkit/game/InputRecord.h src/mealkit/game/InputRecord.cpp)
        if(NOT EXISTS "${engine_game}/${f}")
            message(FATAL_ERROR "simulation version source ${engine_game}/${f} moved: update cmake/MealKit.cmake")
        endif()
        list(APPEND engine_sim_sources "${engine_game}/${f}")
    endforeach()
    list(APPEND engine_sim_sources
        "${MEALKIT_ENGINE_DIR}/third_party/JoltPhysics/mealkit.txt")
    set(build_info_sources ${sim_sources})
    list(SORT build_info_sources)
    list(APPEND build_info_sources "${ARG_HEADLESS}" ${engine_sim_sources})
    string(REPLACE ";" "\n" build_info_list "${build_info_sources}")
    file(CONFIGURE OUTPUT "${build_info_dir}/sim_sources.txt" CONTENT "${build_info_list}\n" @ONLY)
    # Dev builds (and Linux/macOS headless) remember the project folder so an exe built outside it (the engine repo's samples) still finds its assets; shipping builds don't
    set(dev_project_dir "")
    if(MEALKIT_LIVE OR MEALKIT_HEADLESS_ONLY)
        set(dev_project_dir "${project_dir_abs}")
    endif()
    add_custom_target(${name}_buildinfo
        COMMAND ${CMAKE_COMMAND} "-DOUT=${build_info_dir}/mealkit_build_info.h" "-DLIST=${build_info_dir}/sim_sources.txt"
            "-DPROJECT_FILE=${project_dir}/mealkit.project" "-DPROJECT_DIR=${project_dir_abs}" "-DENGINE_DIR=${MEALKIT_ENGINE_DIR}"
            "-DDEV_PROJECT_DIR=${dev_project_dir}"
            -P "${MEALKIT_ENGINE_DIR}/cmake/BuildInfo.cmake"
        BYPRODUCTS "${build_info_dir}/mealkit_build_info.h"
        COMMENT "Build info ${name}: version, commit, simulation version"
        VERBATIM)

    # Whether every character in the string tables (assets/strings/*.txt) exists in the fonts in assets/fonts (windowed builds, mealkit_stringcheck)
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

    # Headless (docs/CODING_MODEL.md "How to run the game without a window (headless/CI)"): sim/ + Headless.cpp only, links only the engine core (mealkit_core). No window/renderer/audio.
    # CI also builds this on Linux/macOS to compare per-tick hashes. Headless.cpp also goes into the shipping exe (--headless, GameSession uses the same functions).
    if(NOT MEALKIT_BUILD_KIND STREQUAL "Editor")
        if(EXISTS "${ARG_HEADLESS}")
            add_executable(${name}_headless "${ARG_HEADLESS}" ${sim_sources})
            target_link_libraries(${name}_headless PRIVATE mealkit_core${engine_suffix})
            target_include_directories(${name}_headless PRIVATE ${source_dir} ${module_includes})
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
        # Game code loaded by the editor. The engine comes from mealkit.dll.
        add_library(${name}_game SHARED ${ARG_MODULE} ${game_sources})
        # <name>_game.dll/.so/.dylib next to the editor (no "lib" prefix; shared libraries go to bin/ on every OS)
        set_target_properties(${name}_game PROPERTIES PREFIX "" LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
        target_link_libraries(${name}_game PRIVATE mealkit ${game_libs})
        target_include_directories(${name}_game PRIVATE ${source_dir} ${module_includes})
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

    # Executable icon: mealkit.project's icon (icon.png if empty, relative to project folder) → multi-size .ico → icon resource 1.
    # Window uses it as the window icon. Without an image, the engine default icon (MealKit) is used.
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
    # Watched with GLOB so it reconfigures if the image is added or removed later
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

    # Game exe: no editor. ImGui (debug UI) only with MEALKIT_DEBUG_UI (off and excluded for Export).
    add_executable(${name} ${ARG_GAME_MAIN} ${game_sources} "${icon_rc}")
    if(icon_found)
        target_sources(${name} PRIVATE "${icon_file}")
    endif()
    target_link_libraries(${name} PRIVATE mealkit${engine_suffix} ${game_libs})
    if(WIN32 AND MSVC AND NOT MEALKIT_DEBUG_UI)
        # Shipping exe (Export) is a GUI program: no console window behind it. main() stays the same; run from a command prompt, attachParentConsole attaches
        target_link_options(${name} PRIVATE /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup)
    endif()
    target_include_directories(${name} PRIVATE ${source_dir} ${module_includes})
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
        # For running during development. Change to your own app ID when shipping; if not 480 it isn't put in the shipping folder.
        file(WRITE ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/steam_appid.txt "${ARG_STEAM_APP_ID}")
    endif()
    if(MEALKIT_EOS_DLL)
        add_custom_command(TARGET ${name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${MEALKIT_EOS_DLL} $<TARGET_FILE_DIR:${name}>)
    endif()

    # Pre-ship checks: folder layout (tools/check_layout.ps1, fails on violation) → merge notices (MealKit + engine libraries in the game exe + game libraries + fonts)
    # → shipping folder (MealKitExport.cmake also checks the exe size budget)
    set(export_notices "${CMAKE_BINARY_DIR}/export_notices/${name}/THIRD_PARTY_NOTICES.txt")
    # Shipping folder: export/<name>, or export/<name>-<version> if mealkit.project's exportVersionFolder is on
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
    # Cook step (game/Cook.h): the dev Release exe (build_project.bat Export builds it first, next to this build folder) packs the reached assets into one pack
    get_filename_component(build_root "${CMAKE_BINARY_DIR}" DIRECTORY)
    set(export_cooker "${build_root}/Release/bin/${name}${CMAKE_EXECUTABLE_SUFFIX}")
    set(export_pack "${build_root}/cook/${name}.pak")
    add_custom_target(${name}_export
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File "${MEALKIT_ENGINE_DIR}/tools/check_layout.ps1"
            -Project "${project_dir}"
        COMMAND "${export_cooker}" --cook "${export_pack}"
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File "${MEALKIT_ENGINE_DIR}/tools/gen_notices.ps1"
            -Project "${project_dir}" -Out "${export_notices}"
        COMMAND ${CMAKE_COMMAND}
            -DGAME_EXE=$<TARGET_FILE:${name}>
            -DPACK=${export_pack}
            -DPROJECT_FILE=${project_dir}/mealkit.project
            -DENGINE_VERSION=${MEALKIT_VERSION}
            -DOUT_DIR=${export_dir}
            -DSTEAM_DLL=${MEALKIT_STEAM_DLL}
            -DEOS_DLL=${MEALKIT_EOS_DLL}
            -DSTEAM_APP_ID=${ARG_STEAM_APP_ID}
            -DNOTICES=${export_notices}
            -DEXE_BUDGET_MB=5
            -P ${MEALKIT_ENGINE_DIR}/cmake/MealKitExport.cmake
        DEPENDS ${name}
        COMMENT "Exporting ${name}"
        VERBATIM)
    # build_project.bat Export builds game_export: every game in this build (the engine repo has one per reference sample)
    if(NOT TARGET game_export)
        add_custom_target(game_export)
    endif()
    add_dependencies(game_export ${name}_export)
endfunction()

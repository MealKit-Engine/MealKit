# 엔진 폴더가 최상위가 아닐 수도 있으므로(게임 프로젝트에서 include) CMAKE_SOURCE_DIR 대신 이 경로를 쓴다.
get_filename_component(MEALKIT_THIRD_PARTY "${CMAKE_CURRENT_SOURCE_DIR}/../third_party" ABSOLUTE)

# --- 셰이더: 빌드 시점에 DXC로 DXIL 바이트 배열 헤더를 만든다 (런타임 컴파일러 의존성 없음) ---
find_program(MEALKIT_DXC dxc HINTS "$ENV{WindowsSdkVerBinPath}/x64" REQUIRED)

set(MEALKIT_SHADER_OUT ${CMAKE_CURRENT_BINARY_DIR}/shaders)
file(MAKE_DIRECTORY ${MEALKIT_SHADER_OUT})

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(MEALKIT_DXC_FLAGS -Zi -Qembed_debug -Od)
else()
    set(MEALKIT_DXC_FLAGS -O3 -Qstrip_debug -Qstrip_reflect)
endif()

set(MEALKIT_SHADER_HEADERS "")
function(mealkit_shader source entry profile name)
    set(out ${MEALKIT_SHADER_OUT}/${name}.h)
    add_custom_command(
        OUTPUT ${out}
        COMMAND ${MEALKIT_DXC} -nologo -T ${profile} -E ${entry} -Vn ${name} -Fh ${out} ${MEALKIT_DXC_FLAGS} ${CMAKE_CURRENT_SOURCE_DIR}/${source}
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${source}
        COMMENT "DXC ${source} (${entry})"
        VERBATIM)
    set(MEALKIT_SHADER_HEADERS ${MEALKIT_SHADER_HEADERS} ${out} PARENT_SCOPE)
endfunction()

mealkit_shader(shaders/Forward.hlsl VSMain vs_6_0 g_ForwardVS)
mealkit_shader(shaders/Forward.hlsl PSMain ps_6_0 g_ForwardPS)

# --- meshoptimizer (모델 최적화/LOD) ---
file(GLOB MEALKIT_MESHOPT_SOURCES ${MEALKIT_THIRD_PARTY}/meshoptimizer/src/*.cpp)
add_library(mealkit_meshopt STATIC ${MEALKIT_MESHOPT_SOURCES})
target_include_directories(mealkit_meshopt SYSTEM PUBLIC ${MEALKIT_THIRD_PARTY}/meshoptimizer/src)
target_compile_options(mealkit_meshopt PRIVATE /W0)

# --- 엔진 라이브러리 ---
# Game 빌드: 정적 라이브러리 (출시 exe에 들어간다). Editor 빌드: mealkit.dll (에디터와 게임 DLL이 같이 쓴다).
if(MEALKIT_BUILD_KIND STREQUAL "Editor")
    set(MEALKIT_LIBRARY_TYPE SHARED)
else()
    set(MEALKIT_LIBRARY_TYPE STATIC)
endif()
add_library(mealkit ${MEALKIT_LIBRARY_TYPE}
    src/mealkit/core/Types.h
    src/mealkit/core/Log.h
    src/mealkit/core/Log.cpp
    src/mealkit/core/Clock.h
    src/mealkit/core/Clock.cpp
    src/mealkit/core/Math.h
    src/mealkit/core/DetMath.h
    src/mealkit/core/Reflect.h
    src/mealkit/core/Reflect.cpp
    src/mealkit/core/Random.h
    src/mealkit/core/Hash.h
    src/mealkit/core/Profile.h
    src/mealkit/game/GameContext.h
    src/mealkit/game/GameInstance.h
    src/mealkit/game/GameData.h
    src/mealkit/game/GameData.cpp
    src/mealkit/game/WorldSnapshot.h
    src/mealkit/sim/Pool.h
    src/mealkit/sim/TickEvents.h
    src/mealkit/sim/TickTimer.h
    src/mealkit/physics/PhysicsWorld.h
    src/mealkit/physics/PhysicsWorld.cpp
    src/mealkit/platform/Input.h
    src/mealkit/platform/Window.h
    src/mealkit/platform/Window.cpp
    src/mealkit/platform/Paths.h
    src/mealkit/platform/Paths.cpp
    src/mealkit/platform/Process.h
    src/mealkit/platform/Process.cpp
    src/mealkit/platform/Steam.h
    src/mealkit/scene/Scene.h
    src/mealkit/scene/Scene.cpp
    src/mealkit/asset/ModelCache.h
    src/mealkit/asset/ModelCache.cpp
    src/mealkit/render/DrawList.h
    src/mealkit/render/Mesh.h
    src/mealkit/render/Mesh.cpp
    src/mealkit/render/Renderer.h
    src/mealkit/render/Renderer.cpp
    src/mealkit/net/Socket.h
    src/mealkit/net/Socket.cpp
    src/mealkit/net/Transport.h
    src/mealkit/net/Lockstep.h
    src/mealkit/net/Lockstep.cpp
    shaders/Forward.hlsl
    ${MEALKIT_SHADER_HEADERS})

set_source_files_properties(shaders/Forward.hlsl PROPERTIES HEADER_FILE_ONLY ON)
# mealkit.dll: 엔진 함수와 클래스를 모두 내보낸다 (__declspec 표시 없이). 게임 코드가 엔진 전역 변수에 직접 접근하지 않는 한 충분하다.
# CMake의 WINDOWS_EXPORT_ALL_SYMBOLS와 같은 방식(__create_def)으로 def를 만들되, MSVC CRT의 약한 별칭이 남기는
# 잘못된 줄("==")을 지운 뒤 링크한다 (WINDOWS_EXPORT_ALL_SYMBOLS 그대로는 LNK2001로 실패한다).
if(MEALKIT_BUILD_KIND STREQUAL "Editor")
    set(mealkit_def ${CMAKE_CURRENT_BINARY_DIR}/mealkit_exports.def)
    set(mealkit_objs ${CMAKE_CURRENT_BINARY_DIR}/mealkit_exports.objs)
    file(GENERATE OUTPUT ${mealkit_objs} CONTENT "$<JOIN:$<TARGET_OBJECTS:mealkit>,\n>\n")
    add_custom_command(TARGET mealkit PRE_LINK
        COMMAND ${CMAKE_COMMAND} -E __create_def ${mealkit_def} ${mealkit_objs}
        COMMAND ${CMAKE_COMMAND} -DDEF_FILE=${mealkit_def} -P ${MEALKIT_ENGINE_DIR}/cmake/FixExportsDef.cmake
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        VERBATIM)
    target_link_options(mealkit PRIVATE /DEF:${mealkit_def})
endif()

target_include_directories(mealkit
    PUBLIC src
    PRIVATE ${MEALKIT_SHADER_OUT})
target_include_directories(mealkit SYSTEM PRIVATE ${MEALKIT_THIRD_PARTY}/cgltf)

target_compile_definitions(mealkit PUBLIC
    NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE
    $<$<CONFIG:Debug>:MEALKIT_DEBUG=1>)
# MEALKIT_ASSERT는 Editor 빌드(에디터 + 게임 DLL)에서도 켠다. PUBLIC이라 MealKitTargets.cmake로 게임 DLL에도 넘어간다.
if(MEALKIT_BUILD_KIND STREQUAL "Editor")
    target_compile_definitions(mealkit PUBLIC MEALKIT_ASSERTS=1)
endif()

# /fp:precise 고정: 시뮬레이션 결정론을 위해 /fp:fast, /fp:contract, /arch:AVX2 를 쓰지 않는다.
target_compile_options(mealkit PUBLIC /W4 /permissive- /utf-8 /Zc:__cplusplus /GR- /fp:precise)

target_link_libraries(mealkit PUBLIC d3d12 dxgi dxguid ws2_32 windowscodecs ole32 uuid shell32 mealkit_tracy PRIVATE Jolt mealkit_meshopt)

# --- SimCheck: 게임 sim/ 폴더의 결정론 규칙 검사 (빌드 도구, mealkit_add_game이 게임 코드보다 먼저 실행한다) ---
add_executable(mealkit_simcheck tools/SimCheck.cpp)
target_compile_options(mealkit_simcheck PRIVATE /W4 /permissive- /utf-8)
set_target_properties(mealkit_simcheck PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tools)

# --- 엔진 단위 테스트: 엔진 저장소 빌드에만 (tools\check.ps1이 Release와 Editor 빌드에서 돌린다) ---
if(MEALKIT_IS_ENGINE)
    add_executable(mealkit_tests tests/Tests.cpp tests/TestRegistry.h)
    target_link_libraries(mealkit_tests PRIVATE mealkit)
endif()

# --- Steamworks (선택): SDK를 third_party/steamworks/sdk 에 풀면 자동으로 켜진다 ---
set(MEALKIT_STEAMWORKS_SDK ${MEALKIT_THIRD_PARTY}/steamworks/sdk)
# 에디터 빌드는 Steam을 쓰지 않는다 (여러 명 플레이 시험은 출시 exe로 한다)
if(NOT MEALKIT_BUILD_KIND STREQUAL "Editor" AND EXISTS ${MEALKIT_STEAMWORKS_SDK}/public/steam/steam_api.h AND EXISTS ${MEALKIT_STEAMWORKS_SDK}/redistributable_bin/win64/steam_api64.lib)
    message(STATUS "MealKit: Steamworks SDK found - Steam lobbies and relay networking enabled")
    target_sources(mealkit PRIVATE src/mealkit/platform/Steam.cpp)
    target_include_directories(mealkit SYSTEM PRIVATE ${MEALKIT_STEAMWORKS_SDK}/public)
    target_link_libraries(mealkit PUBLIC ${MEALKIT_STEAMWORKS_SDK}/redistributable_bin/win64/steam_api64.lib)
    target_compile_definitions(mealkit PUBLIC MEALKIT_STEAM=1)
    # 게임 실행 파일 옆에 복사된다 (mealkit_add_game). steam_appid.txt도 거기서 만든다.
    set(MEALKIT_STEAM_DLL ${MEALKIT_STEAMWORKS_SDK}/redistributable_bin/win64/steam_api64.dll CACHE INTERNAL "")
else()
    if(NOT MEALKIT_BUILD_KIND STREQUAL "Editor")
        message(STATUS "MealKit: Steamworks SDK not found in third_party/steamworks/sdk - building without Steam")
    endif()
    target_sources(mealkit PRIVATE src/mealkit/platform/SteamStub.cpp)
    set(MEALKIT_STEAM_DLL "" CACHE INTERNAL "")
endif()

# --- 범용 에디터 (Editor 빌드): MealKitEditor.exe = mealkit.dll + Dear ImGui + ImGuizmo. 출시 exe에는 들어가지 않는다 ---
if(MEALKIT_BUILD_KIND STREQUAL "Editor")
    add_library(mealkit_imgui STATIC
        ${MEALKIT_THIRD_PARTY}/imgui/imgui.cpp
        ${MEALKIT_THIRD_PARTY}/imgui/imgui_draw.cpp
        ${MEALKIT_THIRD_PARTY}/imgui/imgui_tables.cpp
        ${MEALKIT_THIRD_PARTY}/imgui/imgui_widgets.cpp
        ${MEALKIT_THIRD_PARTY}/imgui/backends/imgui_impl_win32.cpp
        ${MEALKIT_THIRD_PARTY}/imgui/backends/imgui_impl_dx12.cpp
        ${MEALKIT_THIRD_PARTY}/imguizmo/ImGuizmo.cpp)
    target_include_directories(mealkit_imgui PUBLIC
        ${MEALKIT_THIRD_PARTY}/imgui
        ${MEALKIT_THIRD_PARTY}/imgui/backends
        ${MEALKIT_THIRD_PARTY}/imguizmo)
    target_compile_definitions(mealkit_imgui PUBLIC IMGUI_IMPL_WIN32_DISABLE_GAMEPAD PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_options(mealkit_imgui PRIVATE /W0 /utf-8)
    target_link_libraries(mealkit_imgui PUBLIC d3d12 dxgi d3dcompiler dwmapi)

    add_library(mealkit_editor STATIC
        src/mealkit/editor/ImGuiLayer.h
        src/mealkit/editor/ImGuiLayer.cpp
        src/mealkit/editor/Editor.h
        src/mealkit/editor/Editor.cpp
        src/mealkit/editor/Project.h
        src/mealkit/editor/Project.cpp
        src/mealkit/editor/ProjectManager.h
        src/mealkit/editor/ProjectManager.cpp
        src/mealkit/editor/GameModuleHost.h
        src/mealkit/editor/GameModuleHost.cpp
        src/mealkit/editor/FieldEditor.h
        src/mealkit/editor/FieldEditor.cpp
        src/mealkit/editor/PlaySession.h
        src/mealkit/editor/PlaySession.cpp)
    target_link_libraries(mealkit_editor PUBLIC mealkit mealkit_imgui comdlg32)
    target_compile_definitions(mealkit_editor PUBLIC MEALKIT_EDITOR=1)

    add_executable(MealKitEditor src/mealkit/editor/EditorMain.cpp)
    target_link_libraries(MealKitEditor PRIVATE mealkit_editor)
    # 프로젝트 관리자의 새 프로젝트 만들기가 쓰는 엔진 폴더 (에디터는 배포하지 않는다)
    target_compile_definitions(MealKitEditor PRIVATE MEALKIT_ENGINE_DIR="${MEALKIT_ENGINE_DIR}")

    # 게임 프로젝트의 에디터 빌드가 include한다: mealkit.dll의 가져오기 라이브러리, 헤더 경로, 컴파일 옵션, SimCheck
    export(TARGETS mealkit mealkit_tracy mealkit_simcheck FILE ${CMAKE_BINARY_DIR}/MealKitTargets.cmake)
endif()

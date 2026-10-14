# 엔진 폴더가 최상위가 아닐 수도 있으므로(게임 프로젝트에서 include) CMAKE_SOURCE_DIR 대신 이 경로를 쓴다.
get_filename_component(ASTER_THIRD_PARTY "${CMAKE_CURRENT_SOURCE_DIR}/../third_party" ABSOLUTE)

# --- 셰이더: 빌드 시점에 DXC로 DXIL 바이트 배열 헤더를 만든다 (런타임 컴파일러 의존성 없음) ---
find_program(ASTER_DXC dxc HINTS "$ENV{WindowsSdkVerBinPath}/x64" REQUIRED)

set(ASTER_SHADER_OUT ${CMAKE_CURRENT_BINARY_DIR}/shaders)
file(MAKE_DIRECTORY ${ASTER_SHADER_OUT})

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(ASTER_DXC_FLAGS -Zi -Qembed_debug -Od)
else()
    set(ASTER_DXC_FLAGS -O3 -Qstrip_debug -Qstrip_reflect)
endif()

set(ASTER_SHADER_HEADERS "")
function(aster_shader source entry profile name)
    set(out ${ASTER_SHADER_OUT}/${name}.h)
    add_custom_command(
        OUTPUT ${out}
        COMMAND ${ASTER_DXC} -nologo -T ${profile} -E ${entry} -Vn ${name} -Fh ${out} ${ASTER_DXC_FLAGS} ${CMAKE_CURRENT_SOURCE_DIR}/${source}
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${source}
        COMMENT "DXC ${source} (${entry})"
        VERBATIM)
    set(ASTER_SHADER_HEADERS ${ASTER_SHADER_HEADERS} ${out} PARENT_SCOPE)
endfunction()

aster_shader(shaders/Forward.hlsl VSMain vs_6_0 g_ForwardVS)
aster_shader(shaders/Forward.hlsl PSMain ps_6_0 g_ForwardPS)

# --- meshoptimizer (모델 최적화/LOD) ---
file(GLOB ASTER_MESHOPT_SOURCES ${ASTER_THIRD_PARTY}/meshoptimizer/src/*.cpp)
add_library(aster_meshopt STATIC ${ASTER_MESHOPT_SOURCES})
target_include_directories(aster_meshopt SYSTEM PUBLIC ${ASTER_THIRD_PARTY}/meshoptimizer/src)
target_compile_options(aster_meshopt PRIVATE /W0)

# --- 엔진 라이브러리 ---
# Game 빌드: 정적 라이브러리 (출시 exe에 들어간다). Editor 빌드: aster.dll (에디터와 게임 DLL이 같이 쓴다).
if(ASTER_BUILD_KIND STREQUAL "Editor")
    set(ASTER_LIBRARY_TYPE SHARED)
else()
    set(ASTER_LIBRARY_TYPE STATIC)
endif()
add_library(aster ${ASTER_LIBRARY_TYPE}
    src/aster/core/Types.h
    src/aster/core/Log.h
    src/aster/core/Log.cpp
    src/aster/core/Clock.h
    src/aster/core/Clock.cpp
    src/aster/core/Math.h
    src/aster/core/Random.h
    src/aster/core/Hash.h
    src/aster/core/Profile.h
    src/aster/game/GameInstance.h
    src/aster/physics/PhysicsWorld.h
    src/aster/physics/PhysicsWorld.cpp
    src/aster/platform/Input.h
    src/aster/platform/Window.h
    src/aster/platform/Window.cpp
    src/aster/platform/Paths.h
    src/aster/platform/Paths.cpp
    src/aster/platform/Process.h
    src/aster/platform/Process.cpp
    src/aster/platform/Steam.h
    src/aster/scene/Scene.h
    src/aster/scene/Scene.cpp
    src/aster/asset/ModelCache.h
    src/aster/asset/ModelCache.cpp
    src/aster/render/DrawList.h
    src/aster/render/Mesh.h
    src/aster/render/Mesh.cpp
    src/aster/render/Renderer.h
    src/aster/render/Renderer.cpp
    src/aster/net/Socket.h
    src/aster/net/Socket.cpp
    src/aster/net/Transport.h
    src/aster/net/Lockstep.h
    src/aster/net/Lockstep.cpp
    shaders/Forward.hlsl
    ${ASTER_SHADER_HEADERS})

set_source_files_properties(shaders/Forward.hlsl PROPERTIES HEADER_FILE_ONLY ON)
# aster.dll: 엔진 함수와 클래스를 모두 내보낸다 (__declspec 표시 없이). 게임 코드가 엔진 전역 변수에 직접 접근하지 않는 한 충분하다.
# CMake의 WINDOWS_EXPORT_ALL_SYMBOLS와 같은 방식(__create_def)으로 def를 만들되, MSVC CRT의 약한 별칭이 남기는
# 잘못된 줄("==")을 지운 뒤 링크한다 (WINDOWS_EXPORT_ALL_SYMBOLS 그대로는 LNK2001로 실패한다).
if(ASTER_BUILD_KIND STREQUAL "Editor")
    set(aster_def ${CMAKE_CURRENT_BINARY_DIR}/aster_exports.def)
    set(aster_objs ${CMAKE_CURRENT_BINARY_DIR}/aster_exports.objs)
    file(GENERATE OUTPUT ${aster_objs} CONTENT "$<JOIN:$<TARGET_OBJECTS:aster>,\n>\n")
    add_custom_command(TARGET aster PRE_LINK
        COMMAND ${CMAKE_COMMAND} -E __create_def ${aster_def} ${aster_objs}
        COMMAND ${CMAKE_COMMAND} -DDEF_FILE=${aster_def} -P ${ASTER_ENGINE_DIR}/cmake/FixExportsDef.cmake
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        VERBATIM)
    target_link_options(aster PRIVATE /DEF:${aster_def})
endif()

target_include_directories(aster
    PUBLIC src
    PRIVATE ${ASTER_SHADER_OUT})
target_include_directories(aster SYSTEM PRIVATE ${ASTER_THIRD_PARTY}/cgltf)

target_compile_definitions(aster PUBLIC
    NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE
    $<$<CONFIG:Debug>:ASTER_DEBUG=1>)

# /fp:precise 고정: 시뮬레이션 결정론을 위해 /fp:fast, /fp:contract, /arch:AVX2 를 쓰지 않는다.
target_compile_options(aster PUBLIC /W4 /permissive- /utf-8 /Zc:__cplusplus /GR- /fp:precise)

target_link_libraries(aster PUBLIC d3d12 dxgi dxguid ws2_32 windowscodecs ole32 uuid shell32 aster_tracy PRIVATE Jolt aster_meshopt)

# --- Steamworks (선택): SDK를 third_party/steamworks/sdk 에 풀면 자동으로 켜진다 ---
set(ASTER_STEAMWORKS_SDK ${ASTER_THIRD_PARTY}/steamworks/sdk)
# 에디터 빌드는 Steam을 쓰지 않는다 (여러 명 플레이 시험은 출시 exe로 한다)
if(NOT ASTER_BUILD_KIND STREQUAL "Editor" AND EXISTS ${ASTER_STEAMWORKS_SDK}/public/steam/steam_api.h AND EXISTS ${ASTER_STEAMWORKS_SDK}/redistributable_bin/win64/steam_api64.lib)
    message(STATUS "Aster: Steamworks SDK found - Steam lobbies and relay networking enabled")
    target_sources(aster PRIVATE src/aster/platform/Steam.cpp)
    target_include_directories(aster SYSTEM PRIVATE ${ASTER_STEAMWORKS_SDK}/public)
    target_link_libraries(aster PUBLIC ${ASTER_STEAMWORKS_SDK}/redistributable_bin/win64/steam_api64.lib)
    target_compile_definitions(aster PUBLIC ASTER_STEAM=1)
    # 게임 실행 파일 옆에 복사된다 (aster_add_game). steam_appid.txt도 거기서 만든다.
    set(ASTER_STEAM_DLL ${ASTER_STEAMWORKS_SDK}/redistributable_bin/win64/steam_api64.dll CACHE INTERNAL "")
else()
    if(NOT ASTER_BUILD_KIND STREQUAL "Editor")
        message(STATUS "Aster: Steamworks SDK not found in third_party/steamworks/sdk - building without Steam")
    endif()
    target_sources(aster PRIVATE src/aster/platform/SteamStub.cpp)
    set(ASTER_STEAM_DLL "" CACHE INTERNAL "")
endif()

# --- 범용 에디터 (Editor 빌드): AsterEditor.exe = aster.dll + Dear ImGui + ImGuizmo. 출시 exe에는 들어가지 않는다 ---
if(ASTER_BUILD_KIND STREQUAL "Editor")
    add_library(aster_imgui STATIC
        ${ASTER_THIRD_PARTY}/imgui/imgui.cpp
        ${ASTER_THIRD_PARTY}/imgui/imgui_draw.cpp
        ${ASTER_THIRD_PARTY}/imgui/imgui_tables.cpp
        ${ASTER_THIRD_PARTY}/imgui/imgui_widgets.cpp
        ${ASTER_THIRD_PARTY}/imgui/backends/imgui_impl_win32.cpp
        ${ASTER_THIRD_PARTY}/imgui/backends/imgui_impl_dx12.cpp
        ${ASTER_THIRD_PARTY}/imguizmo/ImGuizmo.cpp)
    target_include_directories(aster_imgui PUBLIC
        ${ASTER_THIRD_PARTY}/imgui
        ${ASTER_THIRD_PARTY}/imgui/backends
        ${ASTER_THIRD_PARTY}/imguizmo)
    target_compile_definitions(aster_imgui PUBLIC IMGUI_IMPL_WIN32_DISABLE_GAMEPAD PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_options(aster_imgui PRIVATE /W0 /utf-8)
    target_link_libraries(aster_imgui PUBLIC d3d12 dxgi d3dcompiler dwmapi)

    add_library(aster_editor STATIC
        src/aster/editor/ImGuiLayer.h
        src/aster/editor/ImGuiLayer.cpp
        src/aster/editor/Editor.h
        src/aster/editor/Editor.cpp
        src/aster/editor/Project.h
        src/aster/editor/Project.cpp
        src/aster/editor/ProjectManager.h
        src/aster/editor/ProjectManager.cpp
        src/aster/editor/GameModuleHost.h
        src/aster/editor/GameModuleHost.cpp)
    target_link_libraries(aster_editor PUBLIC aster aster_imgui comdlg32)
    target_compile_definitions(aster_editor PUBLIC ASTER_EDITOR=1)

    add_executable(AsterEditor src/aster/editor/EditorMain.cpp)
    target_link_libraries(AsterEditor PRIVATE aster_editor)
    # 프로젝트 관리자의 새 프로젝트 만들기가 쓰는 엔진 폴더 (에디터는 배포하지 않는다)
    target_compile_definitions(AsterEditor PRIVATE ASTER_ENGINE_DIR="${ASTER_ENGINE_DIR}")

    # 게임 프로젝트의 에디터 빌드가 include한다: aster.dll의 가져오기 라이브러리, 헤더 경로, 컴파일 옵션
    export(TARGETS aster aster_tracy FILE ${CMAKE_BINARY_DIR}/AsterTargets.cmake)
endif()

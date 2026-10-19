# 엔진 폴더가 최상위가 아닐 수도 있으므로(게임 프로젝트에서 include) CMAKE_SOURCE_DIR 대신 이 경로를 쓴다.
get_filename_component(MEALKIT_THIRD_PARTY "${CMAKE_CURRENT_SOURCE_DIR}/../third_party" ABSOLUTE)

# --- 셰이더: 빌드 시점에 DXC로 DXIL 바이트 배열 헤더를 만든다 (런타임 컴파일러 의존성 없음). Windows 화면 쪽만 ---
set(MEALKIT_SHADER_OUT ${CMAKE_CURRENT_BINARY_DIR}/shaders)
set(MEALKIT_SHADER_HEADERS "")
if(NOT MEALKIT_HEADLESS_ONLY)
    find_program(MEALKIT_DXC dxc HINTS "$ENV{WindowsSdkVerBinPath}/x64" REQUIRED)
    file(MAKE_DIRECTORY ${MEALKIT_SHADER_OUT})

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(MEALKIT_DXC_FLAGS -Zi -Qembed_debug -Od)
    else()
        set(MEALKIT_DXC_FLAGS -O3 -Qstrip_debug -Qstrip_reflect)
    endif()

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
    mealkit_shader(shaders/Ui.hlsl VSMain vs_6_0 g_UiVS)
    mealkit_shader(shaders/Ui.hlsl PSMain ps_6_0 g_UiPS)
endif()

# --- meshoptimizer (모델 최적화/LOD) ---
file(GLOB MEALKIT_MESHOPT_SOURCES ${MEALKIT_THIRD_PARTY}/meshoptimizer/src/*.cpp)
add_library(mealkit_meshopt STATIC ${MEALKIT_MESHOPT_SOURCES})
target_include_directories(mealkit_meshopt SYSTEM PUBLIC ${MEALKIT_THIRD_PARTY}/meshoptimizer/src)
if(MSVC)
    target_compile_options(mealkit_meshopt PRIVATE /W0)
else()
    target_compile_options(mealkit_meshopt PRIVATE -w)
endif()

# --- 엔진 라이브러리 ---
# 코어(MEALKIT_CORE_SOURCES): 시뮬레이션·물리·네트워크·데이터·플랫폼 최소 기능. 창·그래픽 없이 돈다 (헤드리스, Linux·macOS도).
# 화면 쪽(MEALKIT_FRONT_SOURCES): 창·입력·렌더러·온라인 서비스·에디터용 플랫폼 기능. Windows만 (다른 OS는 5단계).
# Game 빌드: mealkit_core(정적) + mealkit(정적, 코어를 PUBLIC으로 링크). 출시 exe는 mealkit, <게임>_headless는 mealkit_core만 쓴다.
# Editor 빌드: 둘을 합친 mealkit.dll (에디터와 게임 DLL이 같이 쓴다).
set(MEALKIT_CORE_SOURCES
    src/mealkit/core/Types.h
    src/mealkit/core/Version.h
    src/mealkit/core/Log.h
    src/mealkit/core/Log.cpp
    src/mealkit/core/Clock.h
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
    src/mealkit/data/Tables.h
    src/mealkit/data/Tables.cpp
    src/mealkit/game/Headless.h
    src/mealkit/game/Headless.cpp
    src/mealkit/game/InputRecord.h
    src/mealkit/game/InputRecord.cpp
    src/mealkit/game/ProjectSettings.h
    src/mealkit/game/ProjectSettings.cpp
    src/mealkit/game/Save.h
    src/mealkit/game/Save.cpp
    src/mealkit/game/WorldSnapshot.h
    src/mealkit/game/SimDebug.h
    src/mealkit/game/Simulation.h
    src/mealkit/sim/Pool.h
    src/mealkit/sim/TickEvents.h
    src/mealkit/sim/TickTimer.h
    src/mealkit/sim/Table.h
    src/mealkit/sim/Commands.h
    src/mealkit/input/Actions.h
    src/mealkit/input/Actions.cpp
    src/mealkit/loc/Strings.h
    src/mealkit/loc/Strings.cpp
    src/mealkit/physics/PhysicsWorld.h
    src/mealkit/physics/PhysicsWorld.cpp
    src/mealkit/platform/Paths.h
    src/mealkit/platform/Paths.cpp
    src/mealkit/platform/System.h
    src/mealkit/platform/System.cpp
    src/mealkit/scene/Scene.h
    src/mealkit/scene/Scene.cpp
    src/mealkit/asset/ModelCache.h
    src/mealkit/asset/ModelCache.cpp
    src/mealkit/render/DrawList.h
    src/mealkit/render/Mesh.h
    src/mealkit/render/Mesh.cpp
    src/mealkit/render/Renderer.h
    src/mealkit/net/Socket.h
    src/mealkit/net/Socket.cpp
    src/mealkit/net/Transport.h
    src/mealkit/net/Lockstep.h
    src/mealkit/net/Lockstep.cpp)
set(MEALKIT_FRONT_SOURCES
    src/mealkit/platform/Input.h
    src/mealkit/platform/Gamepad.h
    src/mealkit/platform/Window.h
    src/mealkit/platform/Process.h
    src/mealkit/platform/DynamicLibrary.h
    src/mealkit/platform/FileDialog.h
    src/mealkit/platform/Image.h
    src/mealkit/online/Online.h
    src/mealkit/audio/Audio.h
    src/mealkit/audio/Audio.cpp
    src/mealkit/audio/MiniaudioConfig.h
    src/mealkit/audio/MiniaudioImpl.cpp
    src/mealkit/feedback/Feedback.h
    src/mealkit/feedback/Feedback.cpp
    src/mealkit/ui/Atlas.h
    src/mealkit/ui/Atlas.cpp
    src/mealkit/ui/Document.h
    src/mealkit/ui/Document.cpp
    src/mealkit/ui/Ui.h
    src/mealkit/ui/Ui.cpp
    src/mealkit/ui/StbImpl.cpp
    src/mealkit/settings/Settings.h
    src/mealkit/settings/Settings.cpp
    src/mealkit/game/FrameStats.h
    src/mealkit/game/FrameStats.cpp
    src/mealkit/game/Frontend.h
    src/mealkit/game/Frontend.cpp
    src/mealkit/game/GameMain.h
    src/mealkit/game/GameMain.cpp
    src/mealkit/render/Renderer.cpp
    shaders/Forward.hlsl
    shaders/Ui.hlsl
    ${MEALKIT_SHADER_HEADERS})

# 플랫폼 계층 구현 (docs/CODING_MODEL.md "그래픽 API와 OS 기능"): OS API는 platform/<os>/ 안에서만 부른다.
# posix/는 헤드리스(Linux·macOS)에 필요한 만큼만 있다(시간·파일·UDP·로그). 창·입력·DLL·대화상자·그림은 Windows만 (5단계).
if(WIN32)
    list(APPEND MEALKIT_CORE_SOURCES
        src/mealkit/platform/win32/Clock.cpp
        src/mealkit/platform/win32/Paths.cpp
        src/mealkit/platform/win32/Socket.cpp
        src/mealkit/platform/win32/System.cpp)
    list(APPEND MEALKIT_FRONT_SOURCES
        src/mealkit/platform/win32/DynamicLibrary.cpp
        src/mealkit/platform/win32/Gamepad.cpp
        src/mealkit/platform/win32/FileDialog.cpp
        src/mealkit/platform/win32/Image.cpp
        src/mealkit/platform/win32/Process.cpp
        src/mealkit/platform/win32/Window.cpp)
else()
    list(APPEND MEALKIT_CORE_SOURCES
        src/mealkit/platform/posix/Clock.cpp
        src/mealkit/platform/posix/Paths.cpp
        src/mealkit/platform/posix/Socket.cpp
        src/mealkit/platform/posix/System.cpp)
endif()

# 디버그 UI (MEALKIT_DEBUG_UI, cmake/MealKit.cmake): ImGui 연결 층. 에디터와 개발용 게임 exe의 오버레이가 같이 쓴다
if(MEALKIT_DEBUG_UI)
    list(APPEND MEALKIT_FRONT_SOURCES src/mealkit/debug/ImGuiLayer.h src/mealkit/debug/ImGuiLayer.cpp)
endif()

if(MEALKIT_BUILD_KIND STREQUAL "Editor")
    add_library(mealkit SHARED ${MEALKIT_CORE_SOURCES} ${MEALKIT_FRONT_SOURCES})
    set(mealkit_core_target mealkit)
else()
    add_library(mealkit_core STATIC ${MEALKIT_CORE_SOURCES})
    set(mealkit_core_target mealkit_core)
    if(NOT MEALKIT_HEADLESS_ONLY)
        add_library(mealkit STATIC ${MEALKIT_FRONT_SOURCES})
        target_link_libraries(mealkit PUBLIC mealkit_core)
    endif()
endif()
if(NOT MEALKIT_HEADLESS_ONLY)
    set_source_files_properties(shaders/Forward.hlsl shaders/Ui.hlsl PROPERTIES HEADER_FILE_ONLY ON)
endif()

# mealkit.dll: 엔진 함수와 클래스를 모두 내보낸다 (__declspec 표시 없이). 게임 코드가 엔진 전역 변수에 직접 접근하지 않는 한 충분하다.
# CMake의 WINDOWS_EXPORT_ALL_SYMBOLS와 같은 방식(__create_def)으로 def를 만들되, MSVC CRT의 약한 별칭이 남기는
# 잘못된 줄("==")을 지운 뒤 링크한다 (WINDOWS_EXPORT_ALL_SYMBOLS 그대로는 LNK2001로 실패한다).
if(MEALKIT_BUILD_KIND STREQUAL "Editor")
    set(mealkit_def ${CMAKE_CURRENT_BINARY_DIR}/mealkit_exports.def)
    set(mealkit_objs ${CMAKE_CURRENT_BINARY_DIR}/mealkit_exports.objs)
    file(GENERATE OUTPUT ${mealkit_objs} CONTENT "$<JOIN:$<TARGET_OBJECTS:mealkit>;$<TARGET_OBJECTS:mealkit_imgui>,\n>\n")
    add_custom_command(TARGET mealkit PRE_LINK
        COMMAND ${CMAKE_COMMAND} -E __create_def ${mealkit_def} ${mealkit_objs}
        COMMAND ${CMAKE_COMMAND} -DDEF_FILE=${mealkit_def} -P ${MEALKIT_ENGINE_DIR}/cmake/FixExportsDef.cmake
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        VERBATIM)
    target_link_options(mealkit PRIVATE /DEF:${mealkit_def})
endif()

# 아래 PUBLIC 설정은 코어 타깃에 둔다 (Game 빌드의 mealkit은 코어를 PUBLIC으로 링크해 물려받는다)
target_include_directories(${mealkit_core_target} PUBLIC src)
target_include_directories(${mealkit_core_target} SYSTEM PRIVATE ${MEALKIT_THIRD_PARTY}/cgltf)
if(NOT MEALKIT_HEADLESS_ONLY)
    target_include_directories(mealkit PRIVATE ${MEALKIT_SHADER_OUT})
    # 소리: miniaudio + stb_vorbis (audio/Miniaudio.h). 외부 코드 구현 파일은 경고를 끈다
    target_include_directories(mealkit SYSTEM PRIVATE ${MEALKIT_THIRD_PARTY}/miniaudio ${MEALKIT_THIRD_PARTY}/stb)
    if(MSVC)
        set_source_files_properties(src/mealkit/audio/MiniaudioImpl.cpp src/mealkit/ui/StbImpl.cpp PROPERTIES COMPILE_OPTIONS "/W0")
    endif()
endif()

target_compile_definitions(${mealkit_core_target} PUBLIC
    MEALKIT_VERSION="${MEALKIT_VERSION}"
    MEALKIT_DOCS_URL="${MEALKIT_DOCS_URL}"
    $<$<CONFIG:Debug>:MEALKIT_DEBUG=1>)
if(WIN32)
    target_compile_definitions(${mealkit_core_target} PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
endif()
# MEALKIT_ASSERT는 Editor 빌드(에디터 + 게임 DLL)에서도 켠다. PUBLIC이라 MealKitTargets.cmake로 게임 DLL에도 넘어간다.
if(MEALKIT_BUILD_KIND STREQUAL "Editor")
    target_compile_definitions(mealkit PUBLIC MEALKIT_ASSERTS=1)
endif()

# 부동소수점 결정론 (docs/CODING_MODEL.md "결정론 규칙"): /fp:fast, /fp:contract, /arch:AVX2, -ffast-math를 쓰지 않는다.
# MSVC는 /fp:precise. clang-cl(build.bat ClangCL)은 기본이 precise지만 식 안의 곱셈-덧셈 합치기(fp-contract=on)를 허용하므로 그것만 끈다.
# Linux·macOS의 Clang·GCC(헤드리스)도 -ffp-contract=off, -fno-fast-math.
if(MSVC)
    target_compile_options(${mealkit_core_target} PUBLIC /W4 /permissive- /utf-8 /Zc:__cplusplus /GR-
        $<IF:$<CXX_COMPILER_ID:Clang>,/clang:-ffp-contract=off,/fp:precise>)
else()
    target_compile_options(${mealkit_core_target} PUBLIC -Wall -Wextra -fno-rtti -ffp-contract=off -fno-fast-math)
endif()

target_link_libraries(${mealkit_core_target} PUBLIC mealkit_tracy PRIVATE Jolt mealkit_meshopt)
if(WIN32)
    target_link_libraries(${mealkit_core_target} PUBLIC ws2_32 shell32 ole32)
else()
    find_package(Threads REQUIRED)
    target_link_libraries(${mealkit_core_target} PUBLIC Threads::Threads)
endif()
if(NOT MEALKIT_HEADLESS_ONLY)
    target_link_libraries(mealkit PUBLIC d3d12 dxgi dxguid windowscodecs uuid dwmapi comdlg32 xinput)
endif()

# --- SimCheck: 게임 sim/ 폴더의 결정론 규칙 검사 (빌드 도구, mealkit_add_game이 게임 코드보다 먼저 실행한다) ---
add_executable(mealkit_simcheck tools/SimCheck.cpp)
if(MSVC)
    target_compile_options(mealkit_simcheck PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(mealkit_simcheck PRIVATE -Wall -Wextra)
endif()
set_target_properties(mealkit_simcheck PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tools)

# 문자열 표의 글자가 글꼴에 다 있는지 (게임 빌드 전에, mealkit_add_game). 창 있는 빌드만 (헤드리스는 글자를 그리지 않는다)
if(NOT MEALKIT_HEADLESS_ONLY)
    add_executable(mealkit_stringcheck tools/StringCheck.cpp)
    target_include_directories(mealkit_stringcheck SYSTEM PRIVATE ${MEALKIT_THIRD_PARTY}/stb)
    target_compile_options(mealkit_stringcheck PRIVATE /W4 /permissive- /utf-8)
    set_target_properties(mealkit_stringcheck PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tools)
endif()

# --- 엔진 단위 테스트: 엔진 저장소 빌드에만 (tools\check.ps1이 Release와 Editor 빌드에서 돌린다). 화면 쪽 없이 코어만 쓴다 ---
if(MEALKIT_IS_ENGINE)
    add_executable(mealkit_tests tests/Tests.cpp tests/TestRegistry.h)
    target_link_libraries(mealkit_tests PRIVATE ${mealkit_core_target})
    if(NOT MEALKIT_HEADLESS_ONLY AND NOT MEALKIT_BUILD_KIND STREQUAL "Editor")
        target_link_libraries(mealkit_tests PRIVATE mealkit) # 게임 화면 UI 배치 시험 (창 없이)
        target_compile_definitions(mealkit_tests PRIVATE MEALKIT_TEST_UI=1)
    elseif(MEALKIT_BUILD_KIND STREQUAL "Editor")
        target_compile_definitions(mealkit_tests PRIVATE MEALKIT_TEST_UI=1)
    endif()
endif()

if(MEALKIT_HEADLESS_ONLY)
    return() # Linux·macOS: 헤드리스만 (온라인 서비스·에디터는 5단계)
endif()

# --- 온라인 서비스 (mealkit/online/): 빌드 때 백엔드 하나. 헤드리스는 쓰지 않는다 ---
# Steamworks (선택): SDK를 third_party/steamworks/sdk 에 풀면 자동으로 켜진다
set(MEALKIT_STEAMWORKS_SDK ${MEALKIT_THIRD_PARTY}/steamworks/sdk)
# 에디터 빌드는 Steam을 쓰지 않는다 (여러 명 플레이 시험은 출시 exe로 한다)
if(NOT MEALKIT_BUILD_KIND STREQUAL "Editor" AND EXISTS ${MEALKIT_STEAMWORKS_SDK}/public/steam/steam_api.h AND EXISTS ${MEALKIT_STEAMWORKS_SDK}/redistributable_bin/win64/steam_api64.lib)
    message(STATUS "MealKit: Steamworks SDK found - Steam lobbies and relay networking enabled")
    target_sources(mealkit PRIVATE src/mealkit/online/OnlineSteam.cpp)
    target_include_directories(mealkit SYSTEM PRIVATE ${MEALKIT_STEAMWORKS_SDK}/public)
    target_link_libraries(mealkit PUBLIC ${MEALKIT_STEAMWORKS_SDK}/redistributable_bin/win64/steam_api64.lib)
    target_compile_definitions(mealkit PUBLIC MEALKIT_STEAM=1)
    # 게임 실행 파일 옆에 복사된다 (mealkit_add_game). steam_appid.txt도 거기서 만든다.
    set(MEALKIT_STEAM_DLL ${MEALKIT_STEAMWORKS_SDK}/redistributable_bin/win64/steam_api64.dll CACHE INTERNAL "")
else()
    if(NOT MEALKIT_BUILD_KIND STREQUAL "Editor")
        message(STATUS "MealKit: Steamworks SDK not found in third_party/steamworks/sdk - building without Steam")
    endif()
    target_sources(mealkit PRIVATE src/mealkit/online/OnlineNone.cpp)
    set(MEALKIT_STEAM_DLL "" CACHE INTERNAL "")
endif()

# --- Dear ImGui (MEALKIT_DEBUG_UI): 엔진 라이브러리 안에 넣는다 ---
# Editor 빌드는 mealkit.dll 안(내보내기 def에도)이라 에디터와 게임 DLL이 ImGui 하나·컨텍스트 하나를 같이 쓴다.
# Game 빌드는 정적 mealkit에 들어가 개발용 exe의 디버그 오버레이가 쓴다. ImGuizmo는 에디터만.
if(MEALKIT_DEBUG_UI)
    set(mealkit_imgui_sources
        ${MEALKIT_THIRD_PARTY}/imgui/imgui.cpp
        ${MEALKIT_THIRD_PARTY}/imgui/imgui_draw.cpp
        ${MEALKIT_THIRD_PARTY}/imgui/imgui_tables.cpp
        ${MEALKIT_THIRD_PARTY}/imgui/imgui_widgets.cpp
        ${MEALKIT_THIRD_PARTY}/imgui/backends/imgui_impl_win32.cpp
        ${MEALKIT_THIRD_PARTY}/imgui/backends/imgui_impl_dx12.cpp)
    set(mealkit_imgui_includes ${MEALKIT_THIRD_PARTY}/imgui ${MEALKIT_THIRD_PARTY}/imgui/backends)
    if(MEALKIT_BUILD_KIND STREQUAL "Editor")
        list(APPEND mealkit_imgui_sources ${MEALKIT_THIRD_PARTY}/imguizmo/ImGuizmo.cpp)
        list(APPEND mealkit_imgui_includes ${MEALKIT_THIRD_PARTY}/imguizmo)
    endif()
    # 외부 코드는 엔진 경고·옵션(/W4, UNICODE)과 따로 컴파일한다 (OBJECT 라이브러리 → 목적 파일만 엔진 타깃에)
    add_library(mealkit_imgui OBJECT ${mealkit_imgui_sources})
    target_include_directories(mealkit_imgui PUBLIC ${mealkit_imgui_includes})
    target_compile_definitions(mealkit_imgui PRIVATE IMGUI_IMPL_WIN32_DISABLE_GAMEPAD NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_options(mealkit_imgui PRIVATE /W0 /utf-8)
    target_sources(mealkit PRIVATE $<TARGET_OBJECTS:mealkit_imgui>)
    add_dependencies(mealkit mealkit_imgui)
    target_include_directories(mealkit SYSTEM PUBLIC ${mealkit_imgui_includes})
    target_compile_definitions(mealkit PUBLIC MEALKIT_DEBUG_UI=1 IMGUI_IMPL_WIN32_DISABLE_GAMEPAD)
    target_link_libraries(mealkit PUBLIC d3dcompiler)
endif()

# --- 범용 에디터 (Editor 빌드): MealKitEditor.exe = mealkit.dll(+ Dear ImGui + ImGuizmo). 출시 exe에는 들어가지 않는다 ---
if(MEALKIT_BUILD_KIND STREQUAL "Editor")
    add_library(mealkit_editor STATIC
        src/mealkit/editor/ImGuiLayer.h
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
        src/mealkit/editor/PlaySession.cpp
        src/mealkit/editor/TablesPanel.h
        src/mealkit/editor/TablesPanel.cpp
        src/mealkit/editor/SoundPanel.h
        src/mealkit/editor/SoundPanel.cpp
        src/mealkit/editor/FeedbackPanel.h
        src/mealkit/editor/FeedbackPanel.cpp
        src/mealkit/editor/UiPanel.h
        src/mealkit/editor/UiPanel.cpp
        src/mealkit/editor/Strings.h
        src/mealkit/editor/Strings.cpp)
    target_link_libraries(mealkit_editor PUBLIC mealkit)
    target_compile_definitions(mealkit_editor PUBLIC MEALKIT_EDITOR=1)

    add_executable(MealKitEditor src/mealkit/editor/EditorMain.cpp res/MealKitEditor.rc)
    target_link_libraries(MealKitEditor PRIVATE mealkit_editor)
    # 프로젝트 관리자의 새 프로젝트 만들기가 쓰는 엔진 폴더 (에디터는 배포하지 않는다)
    target_compile_definitions(MealKitEditor PRIVATE MEALKIT_ENGINE_DIR="${MEALKIT_ENGINE_DIR}")

    # 게임 프로젝트의 에디터 빌드가 include한다: mealkit.dll의 가져오기 라이브러리, 헤더 경로, 컴파일 옵션, SimCheck
    export(TARGETS mealkit mealkit_tracy mealkit_simcheck mealkit_stringcheck FILE ${CMAKE_BINARY_DIR}/MealKitTargets.cmake)
endif()

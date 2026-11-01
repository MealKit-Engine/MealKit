# The engine folder may not be top-level (included from a game project), so this path is used instead of CMAKE_SOURCE_DIR.
get_filename_component(MEALKIT_THIRD_PARTY "${CMAKE_CURRENT_SOURCE_DIR}/../third_party" ABSOLUTE)

# --- Shaders: DXC generates byte-array headers at build time (no runtime compiler dependency). Windows front end only ---
# Every build produces both DXIL (DX12) and SPIR-V (Vulkan): a compile failure on either side fails the build (ROADMAP stage 2 "RHI layer").
# This build's backend output goes to shaders/<name>.h (included by the renderer), the other to shaders/other/<name>.h (for checking, not in the exe).
set(MEALKIT_SHADER_OUT ${CMAKE_CURRENT_BINARY_DIR}/shaders)
set(MEALKIT_SHADER_HEADERS "")
if(NOT MEALKIT_HEADLESS_ONLY)
    include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/Dxc.cmake)
    file(MAKE_DIRECTORY ${MEALKIT_SHADER_OUT} ${MEALKIT_SHADER_OUT}/other)

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(MEALKIT_DXIL_FLAGS -Zi -Qembed_debug -Od)
        set(MEALKIT_SPIRV_FLAGS -Od)
    else()
        set(MEALKIT_DXIL_FLAGS -O3 -Qstrip_debug -Qstrip_reflect)
        set(MEALKIT_SPIRV_FLAGS -O3)
    endif()
    list(APPEND MEALKIT_SPIRV_FLAGS -spirv -fspv-target-env=vulkan1.3)

    # Remaining arguments are preprocessor defines (e.g. LIGHTING_MODEL=1 → -D LIGHTING_MODEL=1, style preset variants)
    function(mealkit_shader source entry profile name)
        set(defines "")
        foreach(d ${ARGN})
            list(APPEND defines -D ${d})
        endforeach()
        # The Vulkan backend builds pipelines with entry point names VSMain/PSMain: other entry points (VSOutline etc.) are renamed in SPIR-V
        if(profile MATCHES "^vs_")
            set(spirv_entry -fspv-entrypoint-name=VSMain)
        else()
            set(spirv_entry -fspv-entrypoint-name=PSMain)
        endif()
        if(MEALKIT_RHI STREQUAL "Vulkan")
            set(dxil ${MEALKIT_SHADER_OUT}/other/${name}.h)
            set(spirv ${MEALKIT_SHADER_OUT}/${name}.h)
        else()
            set(dxil ${MEALKIT_SHADER_OUT}/${name}.h)
            set(spirv ${MEALKIT_SHADER_OUT}/other/${name}.h)
        endif()
        set(deps ${CMAKE_CURRENT_SOURCE_DIR}/${source} ${CMAKE_CURRENT_SOURCE_DIR}/shaders/Bindless.hlsli)
        add_custom_command(
            OUTPUT ${dxil}
            COMMAND ${MEALKIT_SHADER_DXC} -nologo -T ${profile} -E ${entry} -Vn ${name} -Fh ${dxil} ${MEALKIT_DXIL_FLAGS} ${defines} ${CMAKE_CURRENT_SOURCE_DIR}/${source}
            DEPENDS ${deps}
            COMMENT "DXC ${source} (${entry}, DXIL)"
            VERBATIM)
        add_custom_command(
            OUTPUT ${spirv}
            COMMAND ${MEALKIT_SHADER_DXC} -nologo -T ${profile} -E ${entry} -Vn ${name} -Fh ${spirv} ${MEALKIT_SPIRV_FLAGS} ${spirv_entry} ${defines} ${CMAKE_CURRENT_SOURCE_DIR}/${source}
            DEPENDS ${deps}
            COMMENT "DXC ${source} (${entry}, SPIR-V)"
            VERBATIM)
        set(MEALKIT_SHADER_HEADERS ${MEALKIT_SHADER_HEADERS} ${dxil} ${spirv} PARENT_SCOPE)
    endfunction()

    # SM 6.6 = minimum spec. Binding layout is shaders/Bindless.hlsli (mealkit/rhi/Rhi.h)
    # One pixel shader per lighting model (render::LightingModel) + an outline pass. All go into the exe and the renderer init builds the pipelines
    mealkit_shader(shaders/Forward.hlsl VSMain vs_6_6 g_ForwardVS)
    mealkit_shader(shaders/Forward.hlsl PSMain ps_6_6 g_ForwardPbrPS LIGHTING_MODEL=0)
    mealkit_shader(shaders/Forward.hlsl PSMain ps_6_6 g_ForwardToonPS LIGHTING_MODEL=1)
    mealkit_shader(shaders/Forward.hlsl PSMain ps_6_6 g_ForwardUnlitPS LIGHTING_MODEL=2)
    mealkit_shader(shaders/Forward.hlsl VSOutline vs_6_6 g_OutlineVS)
    mealkit_shader(shaders/Forward.hlsl PSOutline ps_6_6 g_OutlinePS)
    mealkit_shader(shaders/Forward.hlsl VSShadow vs_6_6 g_ShadowVS)
    mealkit_shader(shaders/Forward.hlsl VSMain vs_6_6 g_ForwardSkinVS SKINNED=1)
    mealkit_shader(shaders/Forward.hlsl VSOutline vs_6_6 g_OutlineSkinVS SKINNED=1)
    mealkit_shader(shaders/Forward.hlsl VSShadow vs_6_6 g_ShadowSkinVS SKINNED=1)
    mealkit_shader(shaders/Forward.hlsl PSShadow ps_6_6 g_ShadowPS)
    mealkit_shader(shaders/Forward.hlsl VSSky vs_6_6 g_SkyVS)
    mealkit_shader(shaders/Forward.hlsl PSSky ps_6_6 g_SkyPS)
    mealkit_shader(shaders/Particle.hlsl VSMain vs_6_6 g_ParticleVS)
    mealkit_shader(shaders/Particle.hlsl PSMain ps_6_6 g_ParticlePS)
    mealkit_shader(shaders/Post.hlsl VSMain vs_6_6 g_PostVS)
    mealkit_shader(shaders/Post.hlsl PSBloomDown ps_6_6 g_BloomDownPS)
    mealkit_shader(shaders/Post.hlsl PSBloomUp ps_6_6 g_BloomUpPS)
    mealkit_shader(shaders/Post.hlsl PSUber ps_6_6 g_UberPS)
    mealkit_shader(shaders/Ui.hlsl VSMain vs_6_6 g_UiVS)
    mealkit_shader(shaders/Ui.hlsl PSMain ps_6_6 g_UiPS)
endif()

# --- meshoptimizer (model optimization/LOD) ---
file(GLOB MEALKIT_MESHOPT_SOURCES ${MEALKIT_THIRD_PARTY}/meshoptimizer/src/*.cpp)
add_library(mealkit_meshopt STATIC ${MEALKIT_MESHOPT_SOURCES})
target_include_directories(mealkit_meshopt SYSTEM PUBLIC ${MEALKIT_THIRD_PARTY}/meshoptimizer/src)
if(MSVC)
    target_compile_options(mealkit_meshopt PRIVATE /W0)
else()
    target_compile_options(mealkit_meshopt PRIVATE -w)
endif()

# --- LZ4 (late-join snapshot compression) ---
set_source_files_properties(${MEALKIT_THIRD_PARTY}/lz4/lz4.c PROPERTIES LANGUAGE CXX) # the project only enables C++
add_library(mealkit_lz4 STATIC ${MEALKIT_THIRD_PARTY}/lz4/lz4.c)
set_target_properties(mealkit_lz4 PROPERTIES LINKER_LANGUAGE CXX)
target_include_directories(mealkit_lz4 SYSTEM PUBLIC ${MEALKIT_THIRD_PARTY}/lz4)
if(MSVC)
    target_compile_options(mealkit_lz4 PRIVATE /W0)
else()
    target_compile_options(mealkit_lz4 PRIVATE -w)
endif()

# --- Engine libraries ---
# Core (MEALKIT_CORE_SOURCES): minimal simulation/physics/network/data/platform features. Runs without a window or graphics (headless, also Linux/macOS).
# Front end (MEALKIT_FRONT_SOURCES): window, input, renderer, online services, platform features for the editor. Windows only (other OSes in stage 5).
# Game build: mealkit_core (static) + mealkit (static, links the core PUBLIC). The shipping exe uses mealkit, <game>_headless only mealkit_core.
# Editor build: both combined into mealkit.dll (shared by the editor and game DLL).
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
    src/mealkit/core/TextFormat.h
    src/mealkit/core/TextFormat.cpp
    src/mealkit/core/Random.h
    src/mealkit/core/Hash.h
    src/mealkit/core/Compress.h
    src/mealkit/core/Compress.cpp
    src/mealkit/core/Zip.h
    src/mealkit/core/Zip.cpp
    src/mealkit/core/Profile.h
    src/mealkit/game/GameContext.h
    src/mealkit/game/GameInstance.h
    src/mealkit/game/BuildInfo.h
    src/mealkit/game/DesyncDump.h
    src/mealkit/game/GameData.h
    src/mealkit/game/GameData.cpp
    src/mealkit/data/Tables.h
    src/mealkit/data/Tables.cpp
    src/mealkit/game/GameOptions.h
    src/mealkit/game/GameOptions.cpp
    src/mealkit/game/Headless.h
    src/mealkit/game/Headless.cpp
    src/mealkit/game/InputRecord.h
    src/mealkit/game/InputRecord.cpp
    src/mealkit/game/Scenario.h
    src/mealkit/game/Scenario.cpp
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
    src/mealkit/sim/MatchFlow.h
    src/mealkit/sim/Movement.h
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
    src/mealkit/platform/Input.h   # header only (key/mouse state, read by input actions)
    src/mealkit/platform/Gamepad.h # PadState is read by input actions. Gamepads implementation is front end (win32/Gamepad.cpp)
    src/mealkit/scene/Scene.h
    src/mealkit/scene/Scene.cpp
    src/mealkit/asset/Budget.h
    src/mealkit/asset/TextureCache.h # header only (texture upload in ModelCache.h). Implementation is front end
    src/mealkit/asset/ModelCache.h
    src/mealkit/asset/Environment.h # declarations only (baking in Environment.cpp is front end, called by the ModelCache::setRenderer inline)
    src/mealkit/asset/ModelCache.cpp
    src/mealkit/asset/Animation.h
    src/mealkit/asset/Animation.cpp
    src/mealkit/asset/AnimStates.h
    src/mealkit/asset/AnimStates.cpp
    src/mealkit/asset/Cutscene.h
    src/mealkit/asset/Cutscene.cpp
    src/mealkit/render/DrawList.h
    src/mealkit/render/Culling.h
    src/mealkit/render/Mesh.h
    src/mealkit/render/Mesh.cpp
    src/mealkit/render/Renderer.h
    src/mealkit/rhi/Rhi.h # header only (GpuInfo in Renderer.h). Backend implementations are front end
    src/mealkit/net/Socket.h
    src/mealkit/net/Socket.cpp
    src/mealkit/net/Transport.h
    src/mealkit/net/Lockstep.h
    src/mealkit/net/Lockstep.cpp
    src/mealkit/net/Conditions.h
    src/mealkit/net/Conditions.cpp
    src/mealkit/net/Rooms.h
    src/mealkit/net/Rooms.cpp)
set(MEALKIT_FRONT_SOURCES
    src/mealkit/platform/Window.h
    src/mealkit/platform/Process.h
    src/mealkit/platform/DynamicLibrary.h
    src/mealkit/platform/FileDialog.h
    src/mealkit/platform/Image.h
    src/mealkit/online/Online.h
    src/mealkit/audio/Audio.h
    src/mealkit/audio/Audio.cpp
    src/mealkit/feedback/Feedback.h
    src/mealkit/feedback/Feedback.cpp
    src/mealkit/camera/Brain.h
    src/mealkit/camera/Brain.cpp
    src/mealkit/ui/Atlas.h
    src/mealkit/ui/Atlas.cpp
    src/mealkit/ui/Document.h
    src/mealkit/ui/Document.cpp
    src/mealkit/ui/Ui.h
    src/mealkit/ui/Text.h
    src/mealkit/ui/Text.cpp
    src/mealkit/ui/Ui.cpp
    src/mealkit/ui/StbImpl.cpp
    src/mealkit/settings/Settings.h
    src/mealkit/settings/Settings.cpp
    src/mealkit/live/Server.h
    src/mealkit/live/Server.cpp
    src/mealkit/game/FrameStats.h
    src/mealkit/game/FrameStats.cpp
    src/mealkit/game/Benchmark.h
    src/mealkit/game/Benchmark.cpp
    src/mealkit/game/Frontend.h
    src/mealkit/game/Frontend.cpp
    src/mealkit/game/RenderStyle.h
    src/mealkit/game/RenderStyle.cpp
    src/mealkit/game/MatchService.h
    src/mealkit/game/MatchService.cpp
    src/mealkit/game/GameMain.h
    src/mealkit/game/GameMain.cpp
    src/mealkit/game/BugReport.h
    src/mealkit/game/BugReport.cpp
    src/mealkit/render/Renderer.cpp
    src/mealkit/asset/TextureCache.cpp
    src/mealkit/asset/Environment.cpp
    src/mealkit/render/ShaderReload.h
    src/mealkit/rhi/GpuCapture.h
    src/mealkit/rhi/GpuCapture.cpp
    src/mealkit/render/ShaderReload.cpp
    shaders/Forward.hlsl
    shaders/Ui.hlsl
    shaders/Bindless.hlsli
    ${MEALKIT_SHADER_HEADERS})

# Platform layer implementations (docs/CODING_MODEL.md "Graphics APIs and OS features"): OS APIs are called only inside platform/<os>/.
# posix/ has only what headless (Linux/macOS) needs (time, files, UDP, logging). Window, input, DLL, dialogs, images are Windows only (stage 5).
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

# RHI backend (MEALKIT_RHI, cmake/MealKit.cmake): graphics APIs are called only inside rhi/<api>/ (tools\check_platform.ps1)
if(MEALKIT_RHI STREQUAL "Vulkan")
    list(APPEND MEALKIT_FRONT_SOURCES
        src/mealkit/rhi/vulkan/Vk.h
        src/mealkit/rhi/vulkan/VkDevice.cpp
        src/mealkit/rhi/vulkan/VkLibs.cpp) # volk + VMA implementation
    set(mealkit_rhi_imgui src/mealkit/rhi/vulkan/VkImGui.cpp)
else()
    list(APPEND MEALKIT_FRONT_SOURCES
        src/mealkit/rhi/dx12/Dx12.h
        src/mealkit/rhi/dx12/Dx12Device.cpp)
    set(mealkit_rhi_imgui src/mealkit/rhi/dx12/Dx12ImGui.cpp)
endif()

# Debug UI (MEALKIT_DEBUG_UI, cmake/MealKit.cmake): ImGui glue layer. Shared by the editor and the dev game exe's overlay
if(MEALKIT_DEBUG_UI)
    list(APPEND MEALKIT_FRONT_SOURCES src/mealkit/debug/ImGuiLayer.h src/mealkit/debug/ImGuiLayer.cpp
        src/mealkit/debug/DebugDraw.h src/mealkit/debug/DebugDrawRender.h src/mealkit/debug/DebugDraw.cpp
        src/mealkit/platform/ImGuiPlatform.h src/mealkit/platform/win32/ImGuiPlatform.cpp
        ${mealkit_rhi_imgui}
        # Texture compression (asset/TextureCache.cpp): dev builds only create it; shipping builds only read the compressed cache
        ${MEALKIT_THIRD_PARTY}/bc7enc/bc7enc.cpp ${MEALKIT_THIRD_PARTY}/bc7enc/rgbcx.cpp)
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
    # Shader hot reload (render/ShaderReload.cpp): dev builds only. Rebuilds this backend's format with the same DXC and options as the build
    if(MEALKIT_DEBUG_UI)
        if(MEALKIT_RHI STREQUAL "Vulkan")
            list(JOIN MEALKIT_SPIRV_FLAGS " " mealkit_hot_flags)
        else()
            list(JOIN MEALKIT_DXIL_FLAGS " " mealkit_hot_flags)
        endif()
        set_source_files_properties(src/mealkit/render/ShaderReload.cpp PROPERTIES COMPILE_DEFINITIONS
            "MEALKIT_SHADER_HOT_RELOAD=1;MEALKIT_SHADER_SOURCE_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/shaders\";MEALKIT_SHADER_HOT_DIR=\"${MEALKIT_SHADER_OUT}/hot\";MEALKIT_SHADER_DXC_PATH=\"${MEALKIT_SHADER_DXC}\";MEALKIT_SHADER_HOT_FLAGS=\"${mealkit_hot_flags}\"")
    endif()
endif()

# mealkit.dll: exports all engine functions and classes (no __declspec markup). Enough as long as game code doesn't access engine globals directly.
# The def is generated the same way as CMake's WINDOWS_EXPORT_ALL_SYMBOLS (__create_def), but the bad lines ("==") left by
# MSVC CRT weak aliases are removed before linking (WINDOWS_EXPORT_ALL_SYMBOLS as-is fails with LNK2001).
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

# The PUBLIC settings below live on the core target (the Game build's mealkit inherits them by linking the core PUBLIC)
target_include_directories(${mealkit_core_target} PUBLIC src)
target_include_directories(${mealkit_core_target} SYSTEM PRIVATE ${MEALKIT_THIRD_PARTY}/cgltf)
# Contract between tools: mkcontract (separate repo MealKit-Contract, version-pinned in third_party/mkcontract). Text format lines, live connection info,
# the editor's tool/project/module lists. Only the library without the client (the engine is the live server; sockets stay in platform/).
# When editing together with the contract: -DMEALKIT_MKCONTRACT_DIR=<MealKit-Contract checkout> (pull it in with tools\update_mkcontract.ps1 when done)
set(MEALKIT_MKCONTRACT_DIR "" CACHE PATH "Local MealKit-Contract checkout (empty uses third_party/mkcontract)")
if(MEALKIT_MKCONTRACT_DIR)
    set(mealkit_mkcontract_dir ${MEALKIT_MKCONTRACT_DIR})
    message(STATUS "MealKit: tool contract from ${MEALKIT_MKCONTRACT_DIR} (local checkout)")
else()
    set(mealkit_mkcontract_dir ${MEALKIT_THIRD_PARTY}/mkcontract)
endif()
set(MKCONTRACT_BUILD_TESTS OFF)
add_subdirectory(${mealkit_mkcontract_dir} ${CMAKE_CURRENT_BINARY_DIR}/mkcontract)
target_link_libraries(${mealkit_core_target} PRIVATE mkcontract)
target_include_directories(${mealkit_core_target} PUBLIC $<BUILD_INTERFACE:${mealkit_mkcontract_dir}/include>)
if(NOT MEALKIT_HEADLESS_ONLY)
    target_include_directories(mealkit PRIVATE ${MEALKIT_SHADER_OUT})
    # Sound: the mkaudio runtime (separate repo MealKit-Audio, version-pinned in third_party/mkaudio; miniaudio + stb_vorbis are the engine's)
    # is wrapped by mealkit/audio/ (glue layer). The game only sees mkaudio header types (Handle etc.) used by mealkit/audio/Audio.h, so only the include path is passed.
    # When editing together with the runtime: -DMEALKIT_MKAUDIO_DIR=<MealKit-Audio checkout> (pull it in with tools\update_mkaudio.ps1 when done)
    set(MEALKIT_MKAUDIO_DIR "" CACHE PATH "Local MealKit-Audio checkout (empty uses third_party/mkaudio)")
    if(MEALKIT_MKAUDIO_DIR)
        set(mealkit_mkaudio_dir ${MEALKIT_MKAUDIO_DIR})
        message(STATUS "MealKit: audio runtime from ${MEALKIT_MKAUDIO_DIR} (local checkout)")
    else()
        set(mealkit_mkaudio_dir ${MEALKIT_THIRD_PARTY}/mkaudio)
    endif()
    set(MKAUDIO_MINIAUDIO_DIR ${MEALKIT_THIRD_PARTY}/miniaudio)
    set(MKAUDIO_STB_DIR ${MEALKIT_THIRD_PARTY}/stb)
    set(MKAUDIO_BUILD_TESTS OFF)
    set(MKAUDIO_BUILD_TOOL OFF) # the authoring tool is built in the MealKit-Audio repo
    add_subdirectory(${mealkit_mkaudio_dir} ${CMAKE_CURRENT_BINARY_DIR}/mkaudio)
    target_link_libraries(mealkit PRIVATE mkaudio)
    target_include_directories(mealkit PUBLIC $<BUILD_INTERFACE:${mealkit_mkaudio_dir}/include>)
    # Disable warnings for external code implementation files
    target_include_directories(mealkit SYSTEM PRIVATE ${MEALKIT_THIRD_PARTY}/stb
                               ${MEALKIT_THIRD_PARTY}/renderdoc # renderdoc_app.h: rhi/GpuCapture.cpp
                               ${MEALKIT_THIRD_PARTY}/bc7enc)   # asset/TextureCache.cpp
    if(MSVC)
        set_source_files_properties(src/mealkit/ui/StbImpl.cpp
            ${MEALKIT_THIRD_PARTY}/bc7enc/bc7enc.cpp ${MEALKIT_THIRD_PARTY}/bc7enc/rgbcx.cpp PROPERTIES COMPILE_OPTIONS "/W0")
    endif()
endif()

target_compile_definitions(${mealkit_core_target} PUBLIC
    MEALKIT_VERSION="${MEALKIT_VERSION}"
    MEALKIT_DOCS_URL="${MEALKIT_DOCS_URL}"
    $<$<CONFIG:Debug>:MEALKIT_DEBUG=1>)
if(WIN32)
    target_compile_definitions(${mealkit_core_target} PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
endif()
# MEALKIT_ASSERT is also on in the Editor build (editor + game DLL). It's PUBLIC so it reaches the game DLL via MealKitTargets.cmake.
if(MEALKIT_BUILD_KIND STREQUAL "Editor")
    target_compile_definitions(mealkit PUBLIC MEALKIT_ASSERTS=1)
endif()
# Live connection (cmake/MealKit.cmake MEALKIT_LIVE): dev build game exes open the server. 0 for shipping builds (Export)
if(MEALKIT_LIVE)
    target_compile_definitions(mealkit PUBLIC MEALKIT_LIVE=1)
else()
    target_compile_definitions(mealkit PUBLIC MEALKIT_LIVE=0)
endif()

# Floating-point determinism (docs/CODING_MODEL.md "Determinism rules"): no /fp:fast, /fp:contract, /arch:AVX2, -ffast-math.
# MSVC uses /fp:precise. clang-cl (build.bat ClangCL) defaults to precise but allows in-expression multiply-add fusion (fp-contract=on), so only that is turned off.
# Clang/GCC on Linux/macOS (headless) also use -ffp-contract=off, -fno-fast-math.
if(MSVC)
    target_compile_options(${mealkit_core_target} PUBLIC /W4 /permissive- /utf-8 /Zc:__cplusplus /GR-
        $<IF:$<CXX_COMPILER_ID:Clang>,/clang:-ffp-contract=off,/fp:precise>)
else()
    target_compile_options(${mealkit_core_target} PUBLIC -Wall -Wextra -fno-rtti -ffp-contract=off -fno-fast-math)
endif()

target_link_libraries(${mealkit_core_target} PUBLIC mealkit_tracy PRIVATE Jolt mealkit_meshopt mealkit_lz4)
if(WIN32)
    target_link_libraries(${mealkit_core_target} PUBLIC ws2_32 shell32 ole32)
else()
    find_package(Threads REQUIRED)
    target_link_libraries(${mealkit_core_target} PUBLIC Threads::Threads)
endif()
if(NOT MEALKIT_HEADLESS_ONLY)
    target_link_libraries(mealkit PUBLIC windowscodecs uuid dwmapi comdlg32 xinput)
    if(MEALKIT_RHI STREQUAL "Vulkan")
        # Built without the SDK: headers are in third_party, the loader (vulkan-1.dll) is loaded at runtime by volk
        set(MEALKIT_VULKAN_INCLUDES ${MEALKIT_THIRD_PARTY}/vulkan-headers/include ${MEALKIT_THIRD_PARTY}/volk ${MEALKIT_THIRD_PARTY}/vma)
        target_include_directories(mealkit SYSTEM PRIVATE ${MEALKIT_VULKAN_INCLUDES})
        target_compile_definitions(mealkit PRIVATE VK_NO_PROTOTYPES VK_USE_PLATFORM_WIN32_KHR)
        if(MSVC)
            set_source_files_properties(src/mealkit/rhi/vulkan/VkLibs.cpp PROPERTIES COMPILE_OPTIONS "/W0")
        endif()
    else()
        target_link_libraries(mealkit PUBLIC d3d12 dxgi dxguid)
    endif()
endif()

# --- SimCheck: determinism rule check for the game's sim/ folder (build tool, mealkit_add_game runs it before the game code) ---
add_executable(mealkit_simcheck tools/SimCheck.cpp)
if(MSVC)
    target_compile_options(mealkit_simcheck PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(mealkit_simcheck PRIVATE -Wall -Wextra)
endif()
set_target_properties(mealkit_simcheck PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tools)

# Whether all characters in the string tables exist in the fonts (before the game build, mealkit_add_game). Windowed builds only (headless draws no text)
if(NOT MEALKIT_HEADLESS_ONLY)
    add_executable(mealkit_stringcheck tools/StringCheck.cpp)
    target_include_directories(mealkit_stringcheck SYSTEM PRIVATE ${MEALKIT_THIRD_PARTY}/stb)
    target_compile_options(mealkit_stringcheck PRIVATE /W4 /permissive- /utf-8)
    set_target_properties(mealkit_stringcheck PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tools)
endif()

# --- Engine unit tests: engine repo build only (tools\check.ps1 runs them in Release and Editor builds). Uses only the core, no front end ---
if(MEALKIT_IS_ENGINE)
    add_executable(mealkit_tests tests/Tests.cpp tests/TestRegistry.h)
    target_link_libraries(mealkit_tests PRIVATE ${mealkit_core_target})
    target_compile_definitions(mealkit_tests PRIVATE MEALKIT_ENGINE_DIR="${MEALKIT_ENGINE_DIR}") # Penpot plugin expected files
    if(NOT MEALKIT_HEADLESS_ONLY AND NOT MEALKIT_BUILD_KIND STREQUAL "Editor")
        target_link_libraries(mealkit_tests PRIVATE mealkit) # game screen UI layout tests (no window)
        target_compile_definitions(mealkit_tests PRIVATE MEALKIT_TEST_UI=1)
    elseif(MEALKIT_BUILD_KIND STREQUAL "Editor")
        target_compile_definitions(mealkit_tests PRIVATE MEALKIT_TEST_UI=1)
    endif()
endif()

if(MEALKIT_HEADLESS_ONLY)
    return() # Linux/macOS: headless only (online services/editor in stage 5)
endif()

# --- Online services (mealkit/online/): one backend per build. Headless doesn't use it ---
# Backend choice: MEALKIT_ONLINE = Auto | Steam | EOS | None. Auto picks Steam if the Steam SDK is present, otherwise EOS if present.
# Crossplay build: -DMEALKIT_ONLINE=EOS (build.bat EOS). If the Steam SDK is also present, it logs into EOS with the Steam account
set(MEALKIT_ONLINE "Auto" CACHE STRING "Online backend: Auto, Steam, EOS, None")
set_property(CACHE MEALKIT_ONLINE PROPERTY STRINGS Auto Steam EOS None)
# Steamworks (optional): usable when the SDK is extracted to third_party/steamworks/sdk
set(MEALKIT_STEAMWORKS_SDK ${MEALKIT_THIRD_PARTY}/steamworks/sdk)
# EOS (optional): the SDK's SDK folder contents in third_party/eos/sdk, and the five Epic Developer Portal values in third_party/eos/credentials.txt
set(MEALKIT_EOS_SDK ${MEALKIT_THIRD_PARTY}/eos/sdk)
set(MEALKIT_EOS_CREDENTIALS ${MEALKIT_THIRD_PARTY}/eos/credentials.txt)
set(mealkit_has_steam OFF)
set(mealkit_has_eos OFF)
if(EXISTS ${MEALKIT_STEAMWORKS_SDK}/public/steam/steam_api.h AND EXISTS ${MEALKIT_STEAMWORKS_SDK}/redistributable_bin/win64/steam_api64.lib)
    set(mealkit_has_steam ON)
endif()
if(EXISTS ${MEALKIT_EOS_SDK}/Include/eos_sdk.h AND EXISTS ${MEALKIT_EOS_SDK}/Lib/EOSSDK-Win64-Shipping.lib AND EXISTS ${MEALKIT_EOS_CREDENTIALS})
    set(mealkit_has_eos ON)
endif()
set(mealkit_online None)
# The editor build doesn't use an online backend (multiplayer testing is done with the shipping exe)
if(NOT MEALKIT_BUILD_KIND STREQUAL "Editor")
    if(MEALKIT_ONLINE STREQUAL "Auto")
        if(mealkit_has_steam)
            set(mealkit_online Steam)
        elseif(mealkit_has_eos)
            set(mealkit_online EOS)
        endif()
    elseif(MEALKIT_ONLINE STREQUAL "Steam" OR MEALKIT_ONLINE STREQUAL "EOS")
        string(TOLOWER ${MEALKIT_ONLINE} mealkit_online_lower)
        if(NOT mealkit_has_${mealkit_online_lower})
            message(FATAL_ERROR "MEALKIT_ONLINE=${MEALKIT_ONLINE} but its SDK (or third_party/eos/credentials.txt) is missing")
        endif()
        set(mealkit_online ${MEALKIT_ONLINE})
    endif()
endif()
set(MEALKIT_STEAM_DLL "" CACHE INTERNAL "")
set(MEALKIT_EOS_DLL "" CACHE INTERNAL "")
if(mealkit_online STREQUAL "Steam")
    message(STATUS "MealKit: online backend Steam - Steam lobbies and relay networking enabled")
    target_sources(mealkit PRIVATE src/mealkit/online/OnlineSteam.cpp)
    target_include_directories(mealkit SYSTEM PRIVATE ${MEALKIT_STEAMWORKS_SDK}/public)
    target_link_libraries(mealkit PUBLIC ${MEALKIT_STEAMWORKS_SDK}/redistributable_bin/win64/steam_api64.lib)
    target_compile_definitions(mealkit PUBLIC MEALKIT_STEAM=1)
    # Copied next to the game executable (mealkit_add_game). steam_appid.txt is also made there.
    set(MEALKIT_STEAM_DLL ${MEALKIT_STEAMWORKS_SDK}/redistributable_bin/win64/steam_api64.dll CACHE INTERNAL "")
elseif(mealkit_online STREQUAL "EOS")
    message(STATUS "MealKit: online backend EOS - EOS lobbies and P2P relay enabled")
    # credentials.txt ("name: value" lines) → EosCredentials.h in the build folder. Never goes into the repo
    file(STRINGS ${MEALKIT_EOS_CREDENTIALS} mealkit_eos_lines)
    foreach(key ProductId SandboxId DeploymentId ClientId ClientSecret)
        set(EOS_${key} "")
        foreach(line ${mealkit_eos_lines})
            if(line MATCHES "^[ 	]*${key}[ 	]*[:=][ 	]*([^, 	]+)")
                set(EOS_${key} ${CMAKE_MATCH_1})
            endif()
        endforeach()
        if(EOS_${key} STREQUAL "")
            message(FATAL_ERROR "third_party/eos/credentials.txt has no '${key}: <value>' line")
        endif()
    endforeach()
    configure_file(src/mealkit/online/EosCredentials.h.in ${CMAKE_CURRENT_BINARY_DIR}/generated/EosCredentials.h @ONLY)
    target_sources(mealkit PRIVATE src/mealkit/online/OnlineEOS.cpp)
    target_include_directories(mealkit SYSTEM PRIVATE ${MEALKIT_EOS_SDK}/Include ${CMAKE_CURRENT_BINARY_DIR}/generated)
    target_link_libraries(mealkit PUBLIC ${MEALKIT_EOS_SDK}/Lib/EOSSDK-Win64-Shipping.lib)
    target_compile_definitions(mealkit PUBLIC MEALKIT_EOS=1)
    set(MEALKIT_EOS_DLL ${MEALKIT_EOS_SDK}/Bin/EOSSDK-Win64-Shipping.dll CACHE INTERNAL "")
    # If the Steam SDK is also present: EOS login with the Steam account + Steam friend invites (carrying the lobby code). Match transport is EOS only
    if(mealkit_has_steam)
        message(STATUS "MealKit: Steamworks SDK found - EOS logs in with the Steam account, invites go through Steam")
        target_include_directories(mealkit SYSTEM PRIVATE ${MEALKIT_STEAMWORKS_SDK}/public)
        target_link_libraries(mealkit PUBLIC ${MEALKIT_STEAMWORKS_SDK}/redistributable_bin/win64/steam_api64.lib)
        target_compile_definitions(mealkit PUBLIC MEALKIT_EOS_STEAM=1)
        set(MEALKIT_STEAM_DLL ${MEALKIT_STEAMWORKS_SDK}/redistributable_bin/win64/steam_api64.dll CACHE INTERNAL "")
    endif()
else()
    if(NOT MEALKIT_BUILD_KIND STREQUAL "Editor")
        message(STATUS "MealKit: no online SDK (third_party/steamworks/sdk, third_party/eos/sdk) - building without online backend")
    endif()
    target_sources(mealkit PRIVATE src/mealkit/online/OnlineNone.cpp)
endif()

# --- Dear ImGui (MEALKIT_DEBUG_UI): built into the engine library ---
# In the Editor build it's inside mealkit.dll (also in the export def), so the editor and game DLL share one ImGui and one context.
# In the Game build it goes into static mealkit and is used by the dev exe's debug overlay. ImGuizmo is editor only.
if(MEALKIT_DEBUG_UI)
    set(mealkit_imgui_sources
        ${MEALKIT_THIRD_PARTY}/imgui/imgui.cpp
        ${MEALKIT_THIRD_PARTY}/imgui/imgui_draw.cpp
        ${MEALKIT_THIRD_PARTY}/imgui/imgui_tables.cpp
        ${MEALKIT_THIRD_PARTY}/imgui/imgui_widgets.cpp
        ${MEALKIT_THIRD_PARTY}/imgui/backends/imgui_impl_win32.cpp)
    set(mealkit_imgui_includes ${MEALKIT_THIRD_PARTY}/imgui ${MEALKIT_THIRD_PARTY}/imgui/backends)
    if(MEALKIT_RHI STREQUAL "Vulkan")
        list(APPEND mealkit_imgui_sources ${MEALKIT_THIRD_PARTY}/imgui/backends/imgui_impl_vulkan.cpp)
    else()
        list(APPEND mealkit_imgui_sources ${MEALKIT_THIRD_PARTY}/imgui/backends/imgui_impl_dx12.cpp)
    endif()
    if(MEALKIT_BUILD_KIND STREQUAL "Editor")
        list(APPEND mealkit_imgui_sources ${MEALKIT_THIRD_PARTY}/imguizmo/ImGuizmo.cpp)
        list(APPEND mealkit_imgui_includes ${MEALKIT_THIRD_PARTY}/imguizmo)
    endif()
    # External code is compiled separately from engine warnings/options (/W4, UNICODE) (OBJECT library → only object files into the engine target)
    add_library(mealkit_imgui OBJECT ${mealkit_imgui_sources})
    target_include_directories(mealkit_imgui PUBLIC ${mealkit_imgui_includes})
    target_compile_definitions(mealkit_imgui PRIVATE IMGUI_IMPL_WIN32_DISABLE_GAMEPAD NOMINMAX WIN32_LEAN_AND_MEAN)
    if(MEALKIT_RHI STREQUAL "Vulkan")
        target_include_directories(mealkit_imgui SYSTEM PRIVATE ${MEALKIT_VULKAN_INCLUDES})
        target_compile_definitions(mealkit_imgui PRIVATE IMGUI_IMPL_VULKAN_USE_VOLK VK_NO_PROTOTYPES VK_USE_PLATFORM_WIN32_KHR)
    endif()
    target_compile_options(mealkit_imgui PRIVATE /W0 /utf-8)
    target_sources(mealkit PRIVATE $<TARGET_OBJECTS:mealkit_imgui>)
    add_dependencies(mealkit mealkit_imgui)
    target_include_directories(mealkit SYSTEM PUBLIC ${mealkit_imgui_includes})
    target_compile_definitions(mealkit PUBLIC MEALKIT_DEBUG_UI=1 IMGUI_IMPL_WIN32_DISABLE_GAMEPAD)
    if(NOT MEALKIT_RHI STREQUAL "Vulkan")
        target_link_libraries(mealkit PUBLIC d3dcompiler) # shaders for the ImGui DX12 backend
    endif()
endif()

# --- General-purpose editor (Editor build): MealKitEditor.exe = mealkit.dll (+ Dear ImGui + ImGuizmo). Not in the shipping exe ---
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
        src/mealkit/editor/LookdevPanel.h
        src/mealkit/editor/LookdevPanel.cpp
        src/mealkit/editor/ImportServer.h
        src/mealkit/editor/ImportServer.cpp
        src/mealkit/editor/Tools.h
        src/mealkit/editor/Tools.cpp
        src/mealkit/editor/UiCheck.h
        src/mealkit/editor/UiCheck.cpp
        src/mealkit/editor/TuningPanel.h
        src/mealkit/editor/TuningPanel.cpp
        src/mealkit/editor/ProjectSettingsPanel.h
        src/mealkit/editor/ProjectSettingsPanel.cpp
        src/mealkit/editor/BuildPanel.h
        src/mealkit/editor/BuildPanel.cpp
        src/mealkit/editor/ModulesPanel.cpp
        src/mealkit/editor/LogPanel.h
        src/mealkit/editor/LogPanel.cpp
        src/mealkit/editor/EditorCommands.cpp
        src/mealkit/editor/EditorAssets.cpp
        src/mealkit/editor/Preferences.h
        src/mealkit/editor/Preferences.cpp
        src/mealkit/editor/PerfPanel.h
        src/mealkit/editor/PerfPanel.cpp
        src/mealkit/editor/CodeLink.h
        src/mealkit/editor/CodeLink.cpp
        src/mealkit/editor/Strings.h
        src/mealkit/editor/Strings.cpp)
    target_link_libraries(mealkit_editor PUBLIC mealkit PRIVATE mkcontract) # mealkit.dll exports only its own objects, not mkcontract's
    target_compile_definitions(mealkit_editor PUBLIC MEALKIT_EDITOR=1)

    add_executable(MealKitEditor src/mealkit/editor/EditorMain.cpp res/MealKitEditor.rc)
    target_link_libraries(MealKitEditor PRIVATE mealkit_editor)
    # GUI program: no console window behind it (run from a command prompt, attachParentConsole attaches)
    target_link_options(MealKitEditor PRIVATE /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup)
    # Engine folder used by the project manager's new project creation (the editor isn't distributed)
    target_compile_definitions(MealKitEditor PRIVATE MEALKIT_ENGINE_DIR="${MEALKIT_ENGINE_DIR}")

    # Included by a game project's editor build: mealkit.dll import library, header paths, compile options, SimCheck
    export(TARGETS mealkit mealkit_tracy mealkit_simcheck mealkit_stringcheck FILE ${CMAKE_BINARY_DIR}/MealKitTargets.cmake)
endif()

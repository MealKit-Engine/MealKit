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

    # Remaining arguments are preprocessor defines (e.g. LIGHTING_MODEL=1 ??-D LIGHTING_MODEL=1, style preset variants)
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
    core/core/include/mealkit/core/Types.h
    core/core/include/mealkit/core/Version.h
    core/core/include/mealkit/core/Log.h
    core/core/src/mealkit/core/Log.cpp
    core/core/include/mealkit/core/Clock.h
    core/core/include/mealkit/core/Math.h
    core/core/include/mealkit/core/DetMath.h
    core/core/include/mealkit/core/Reflect.h
    core/core/src/mealkit/core/Reflect.cpp
    core/core/include/mealkit/core/TextFormat.h
    core/core/src/mealkit/core/TextFormat.cpp
    core/core/include/mealkit/core/Random.h
    core/core/include/mealkit/core/Hash.h
    core/core/include/mealkit/core/Compress.h
    core/core/src/mealkit/core/Compress.cpp
    core/core/include/mealkit/core/Zip.h
    core/core/src/mealkit/core/Zip.cpp
    core/core/include/mealkit/core/Profile.h
    core/core/include/mealkit/core/Memory.h
    core/core/src/mealkit/core/Memory.cpp
    core/core/include/mealkit/core/Services.h
    core/core/src/mealkit/core/Services.cpp
    core/core/include/mealkit/core/Modules.h
    core/core/src/mealkit/core/Modules.cpp
    ../modules/game/include/mealkit/game/GameContext.h
    ../modules/game/include/mealkit/game/GameInstance.h
    ../modules/game/include/mealkit/game/BuildInfo.h
    ../modules/game/include/mealkit/game/DesyncDump.h
    ../modules/game/include/mealkit/game/GameData.h
    ../modules/game/src/mealkit/game/GameData.cpp
    ../modules/data/include/mealkit/data/Tables.h
    ../modules/data/src/mealkit/data/Tables.cpp
    ../modules/game/include/mealkit/game/GameOptions.h
    ../modules/game/src/mealkit/game/GameOptions.cpp
    ../modules/game/include/mealkit/game/Headless.h
    ../modules/game/src/mealkit/game/Headless.cpp
    ../modules/game/include/mealkit/game/InputRecord.h
    ../modules/game/src/mealkit/game/InputRecord.cpp
    ../modules/game/include/mealkit/game/Scenario.h
    ../modules/game/src/mealkit/game/Scenario.cpp
    ../modules/game/include/mealkit/game/ProjectSettings.h
    ../modules/game/src/mealkit/game/ProjectSettings.cpp
    ../modules/game/include/mealkit/game/Save.h
    ../modules/game/src/mealkit/game/Save.cpp
    ../modules/game/include/mealkit/game/WorldSnapshot.h
    ../modules/game/include/mealkit/game/SimDebug.h
    ../modules/game/include/mealkit/game/Simulation.h
    ../modules/sim/include/mealkit/sim/Pool.h
    ../modules/sim/include/mealkit/sim/TickEvents.h
    ../modules/sim/include/mealkit/sim/TickTimer.h
    ../modules/sim/include/mealkit/sim/MatchFlow.h
    ../modules/sim/include/mealkit/sim/Movement.h
    ../modules/sim/include/mealkit/sim/Ragdoll.h
    ../modules/sim/include/mealkit/sim/NavGrid.h
    ../modules/sim/include/mealkit/sim/Table.h
    ../modules/sim/include/mealkit/sim/Commands.h
    ../modules/input/include/mealkit/input/Actions.h
    ../modules/input/src/mealkit/input/Actions.cpp
    ../modules/loc/include/mealkit/loc/Strings.h
    ../modules/loc/src/mealkit/loc/Strings.cpp
    core/jobs/include/mealkit/jobs/Jobs.h
    core/jobs/src/mealkit/jobs/Jobs.cpp
    core/jobs/include/mealkit/jobs/JoltAllocator.h
    core/jobs/src/mealkit/jobs/JoltAllocator.cpp
    ../modules/physics/include/mealkit/physics/PhysicsWorld.h
    ../modules/physics/src/mealkit/physics/PhysicsWorld.cpp
    core/platform/include/mealkit/platform/Paths.h
    core/platform/src/mealkit/platform/Paths.cpp
    core/platform/include/mealkit/platform/Pack.h
    core/platform/src/mealkit/platform/Pack.cpp
    core/platform/include/mealkit/platform/System.h
    core/platform/src/mealkit/platform/System.cpp
    core/platform/include/mealkit/platform/Input.h   # header only (key/mouse state, read by input actions)
    core/platform/include/mealkit/platform/Gamepad.h # PadState is read by input actions. Gamepads implementation is front end (win32/Gamepad.cpp)
    ../modules/scene/include/mealkit/scene/Scene.h
    ../modules/scene/src/mealkit/scene/Scene.cpp
    ../modules/asset/include/mealkit/asset/Budget.h
    ../modules/asset/include/mealkit/asset/TextureCache.h # header only (texture upload in ModelCache.h). Implementation is front end
    ../modules/asset/include/mealkit/asset/ModelCache.h
    ../modules/asset/include/mealkit/asset/Environment.h # declarations only (baking in Environment.cpp is front end, called by the ModelCache::setRenderer inline)
    ../modules/asset/src/mealkit/asset/ModelCache.cpp
    ../modules/asset/include/mealkit/asset/Cooked.h
    ../modules/asset/src/mealkit/asset/Cooked.cpp
    ../modules/asset/src/mealkit/asset/ModelImport.cpp # glTF import, only with MEALKIT_ASSET_IMPORT (empty in shipping builds)
    ../modules/asset/include/mealkit/asset/Terrain.h
    ../modules/asset/include/mealkit/asset/Scatter.h
    ../modules/asset/src/mealkit/asset/Scatter.cpp
    ../modules/asset/src/mealkit/asset/Terrain.cpp # heightmap PNG decode/encode (static stb_image + stb_image_write copies, headless too)
    ../modules/asset/include/mealkit/asset/Animation.h
    ../modules/asset/src/mealkit/asset/Animation.cpp
    ../modules/asset/include/mealkit/asset/AnimStates.h
    ../modules/asset/src/mealkit/asset/AnimStates.cpp
    ../modules/asset/include/mealkit/asset/Cutscene.h
    ../modules/asset/src/mealkit/asset/Cutscene.cpp
    ../modules/render/include/mealkit/render/DrawList.h
    ../modules/render/include/mealkit/render/Culling.h
    ../modules/render/include/mealkit/render/Mesh.h
    ../modules/render/src/mealkit/render/Mesh.cpp
    ../modules/render/include/mealkit/render/Renderer.h
    ../modules/rhi/include/mealkit/rhi/Rhi.h # header only (GpuInfo in Renderer.h). Backend implementations are front end
    core/platform/include/mealkit/platform/Socket.h
    ../modules/session/include/mealkit/session/Session.h
    ../modules/session/include/mealkit/session/LocalSession.h
    ../modules/session/src/mealkit/session/Session.cpp
    ../modules/session/src/mealkit/session/LocalSession.cpp
    core/platform/src/mealkit/platform/Socket.cpp
    ../modules/net/include/mealkit/net/Transport.h
    ../modules/net/include/mealkit/net/Lockstep.h
    ../modules/net/src/mealkit/net/Lockstep.cpp
    ../modules/net/include/mealkit/net/Conditions.h
    ../modules/net/src/mealkit/net/Conditions.cpp
    ../modules/net/include/mealkit/net/Rooms.h
    ../modules/net/src/mealkit/net/Rooms.cpp)
set(MEALKIT_FRONT_SOURCES
    core/platform/include/mealkit/platform/Window.h
    core/platform/include/mealkit/platform/Process.h
    core/platform/include/mealkit/platform/DynamicLibrary.h
    core/platform/include/mealkit/platform/FileDialog.h
    core/platform/include/mealkit/platform/Image.h
    ../modules/online/include/mealkit/online/Online.h
    ../modules/audio/include/mealkit/audio/Audio.h
    ../modules/audio/src/mealkit/audio/Audio.cpp
    ../modules/feedback/include/mealkit/feedback/Feedback.h
    ../modules/feedback/src/mealkit/feedback/Feedback.cpp
    ../modules/camera/include/mealkit/camera/Brain.h
    ../modules/camera/src/mealkit/camera/Brain.cpp
    ../modules/ui/include/mealkit/ui/Document.h
    ../modules/ui/include/mealkit/ui/Ui.h
    ../modules/ui/src/mealkit/ui/Ui.cpp
    ../modules/ui/src/mealkit/ui/StbImpl.cpp
    ../modules/settings/include/mealkit/settings/Settings.h
    ../modules/settings/src/mealkit/settings/Settings.cpp
    ../modules/live/include/mealkit/live/Server.h
    ../modules/live/src/mealkit/live/Server.cpp
    ../modules/game/include/mealkit/game/FrameStats.h
    ../modules/game/src/mealkit/game/FrameStats.cpp
    ../modules/game/src/mealkit/game/Benchmark.h
    ../modules/game/src/mealkit/game/Benchmark.cpp
    ../modules/game/include/mealkit/game/Frontend.h
    ../modules/game/src/mealkit/game/Frontend.cpp
    ../modules/game/include/mealkit/game/RenderStyle.h
    ../modules/game/src/mealkit/game/RenderStyle.cpp
    ../modules/game/include/mealkit/game/MatchService.h
    ../modules/game/src/mealkit/game/MatchService.cpp
    ../modules/game/include/mealkit/game/GameMain.h
    ../modules/game/src/mealkit/game/GameMain.cpp
    ../modules/game/include/mealkit/game/BugReport.h
    ../modules/game/src/mealkit/game/BugReport.cpp
    ../modules/game/src/mealkit/game/Cook.h
    ../modules/game/src/mealkit/game/Cook.cpp
    ../modules/render/src/mealkit/render/Renderer.cpp
    ../modules/asset/src/mealkit/asset/TextureCache.cpp
    ../modules/asset/src/mealkit/asset/Environment.cpp
    ../modules/render/src/mealkit/render/ShaderReload.h
    ../modules/rhi/include/mealkit/rhi/GpuCapture.h
    ../modules/rhi/src/mealkit/rhi/GpuCapture.cpp
    ../modules/render/src/mealkit/render/ShaderReload.cpp
    shaders/Forward.hlsl
    shaders/Ui.hlsl
    shaders/Bindless.hlsli
    ${MEALKIT_SHADER_HEADERS})

# Platform layer implementations (docs/CODING_MODEL.md "Graphics APIs and OS features"): OS APIs are called only inside platform/<os>/.
# posix/ has what headless (Linux/macOS) needs (time, files, UDP, logging) and dynamic libraries. Window, input, dialogs, images are Windows only (stage 5).
if(WIN32)
    list(APPEND MEALKIT_CORE_SOURCES
        core/platform/src/mealkit/platform/win32/Clock.cpp
        core/platform/src/mealkit/platform/win32/Paths.cpp
        core/platform/src/mealkit/platform/win32/Socket.cpp
        core/platform/src/mealkit/platform/win32/System.cpp)
    list(APPEND MEALKIT_FRONT_SOURCES
        core/platform/src/mealkit/platform/win32/DynamicLibrary.cpp
        core/platform/src/mealkit/platform/win32/Gamepad.cpp
        core/platform/src/mealkit/platform/win32/FileDialog.cpp
        core/platform/src/mealkit/platform/win32/Image.cpp
        core/platform/src/mealkit/platform/win32/Process.cpp
        core/platform/src/mealkit/platform/win32/Window.cpp)
else()
    list(APPEND MEALKIT_CORE_SOURCES
        core/platform/src/mealkit/platform/posix/Clock.cpp
        core/platform/src/mealkit/platform/posix/Paths.cpp
        core/platform/src/mealkit/platform/posix/Socket.cpp
        core/platform/src/mealkit/platform/posix/System.cpp)
    list(APPEND MEALKIT_FRONT_SOURCES core/platform/src/mealkit/platform/posix/DynamicLibrary.cpp)
endif()

# RHI backend (MEALKIT_RHI, cmake/MealKit.cmake): graphics APIs are called only inside rhi/<api>/ (tools\check_platform.ps1)
if(MEALKIT_RHI STREQUAL "Vulkan")
    list(APPEND MEALKIT_FRONT_SOURCES
        ../modules/rhi/src/mealkit/rhi/vulkan/Vk.h
        ../modules/rhi/src/mealkit/rhi/vulkan/VkDevice.cpp
        ../modules/rhi/src/mealkit/rhi/vulkan/VkLibs.cpp) # volk + VMA implementation
    set(mealkit_rhi_imgui ../modules/rhi/src/mealkit/rhi/vulkan/VkImGui.cpp)
else()
    list(APPEND MEALKIT_FRONT_SOURCES
        ../modules/rhi/src/mealkit/rhi/dx12/Dx12.h
        ../modules/rhi/src/mealkit/rhi/dx12/Dx12Device.cpp)
    set(mealkit_rhi_imgui ../modules/rhi/src/mealkit/rhi/dx12/Dx12ImGui.cpp)
endif()

# Debug UI (MEALKIT_DEBUG_UI, cmake/MealKit.cmake): ImGui glue layer. Shared by the editor and the dev game exe's overlay
if(MEALKIT_DEBUG_UI)
    list(APPEND MEALKIT_FRONT_SOURCES ../modules/debug/include/mealkit/debug/ImGuiLayer.h ../modules/debug/src/mealkit/debug/ImGuiLayer.cpp
        ../modules/debug/include/mealkit/debug/DebugDraw.h ../modules/debug/include/mealkit/debug/DebugDrawRender.h ../modules/debug/src/mealkit/debug/DebugDraw.cpp
        core/core/include/mealkit/core/AllocCount.h core/core/src/mealkit/core/AllocCount.cpp
        core/platform/include/mealkit/platform/ImGuiPlatform.h core/platform/src/mealkit/platform/win32/ImGuiPlatform.cpp
        ${mealkit_rhi_imgui}
        # Texture compression (asset/TextureCache.cpp): dev builds only create it; shipping builds only read the compressed cache
        ${MEALKIT_THIRD_PARTY}/bc7enc/bc7enc.cpp ${MEALKIT_THIRD_PARTY}/bc7enc/rgbcx.cpp)
endif()

# Frame allocation counter (core/AllocCount.h): replaces operator new only in the dev game exe (static), not in mealkit.dll
if(MEALKIT_DEBUG_UI AND NOT MEALKIT_BUILD_KIND STREQUAL "Editor")
    set_source_files_properties(core/core/src/mealkit/core/AllocCount.cpp PROPERTIES COMPILE_DEFINITIONS MEALKIT_ALLOC_COUNT=1)
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
        set_source_files_properties(../modules/render/src/mealkit/render/ShaderReload.cpp PROPERTIES COMPILE_DEFINITIONS
            "MEALKIT_SHADER_HOT_RELOAD=1;MEALKIT_SHADER_SOURCE_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/shaders\";MEALKIT_SHADER_HOT_DIR=\"${MEALKIT_SHADER_OUT}/hot\";MEALKIT_SHADER_DXC_PATH=\"${MEALKIT_SHADER_DXC}\";MEALKIT_SHADER_HOT_FLAGS=\"${mealkit_hot_flags}\"")
    endif()
endif()

# mealkit.dll: exports all engine functions and classes (no __declspec markup). Enough as long as game code doesn't access engine globals directly.
# The def is generated the same way as CMake's WINDOWS_EXPORT_ALL_SYMBOLS (__create_def), but the bad lines ("==") left by
# MSVC CRT weak aliases are removed before linking (WINDOWS_EXPORT_ALL_SYMBOLS as-is fails with LNK2001).
# Clang/GCC on Linux/macOS get the same result with default symbol visibility: libmealkit.so/.dylib exports everything, no def file
if(MEALKIT_BUILD_KIND STREQUAL "Editor" AND NOT WIN32)
    set_target_properties(mealkit PROPERTIES CXX_VISIBILITY_PRESET default VISIBILITY_INLINES_HIDDEN OFF)
    target_link_libraries(mealkit PRIVATE ${CMAKE_DL_LIBS}) # dlopen (platform/posix/DynamicLibrary.cpp)
elseif(MEALKIT_BUILD_KIND STREQUAL "Editor")
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
# Engine modules (docs/ENGINE_MODULES.md): the kernel in engine/core/<name>/, the rest in modules/<name>/, each with an engine-module.txt.
# include/ holds the headers other modules and games may include (still "mealkit/<module>/X.h"), src/ the module's own files and private headers.
file(GLOB mealkit_module_manifests CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/core/*/engine-module.txt ${CMAKE_CURRENT_SOURCE_DIR}/../modules/*/engine-module.txt)
set(MEALKIT_MODULE_INCLUDES "")
set(MEALKIT_MODULE_SOURCES_DIRS "")
set(MEALKIT_MODULE_EDITOR_DIRS "")
foreach(manifest ${mealkit_module_manifests})
    get_filename_component(module_dir ${manifest} DIRECTORY)
    get_filename_component(module_dir ${module_dir} ABSOLUTE)
    if(EXISTS ${module_dir}/include)
        list(APPEND MEALKIT_MODULE_INCLUDES ${module_dir}/include)
    endif()
    list(APPEND MEALKIT_MODULE_SOURCES_DIRS ${module_dir}/src)
    if(EXISTS ${module_dir}/editor) # the module's editor panels (docs/ENGINE_MODULES.md "Module layout"): mealkit_editor only
        list(APPEND MEALKIT_MODULE_EDITOR_DIRS ${module_dir}/editor)
    endif()
endforeach()
target_include_directories(${mealkit_core_target} PUBLIC ${MEALKIT_MODULE_INCLUDES} PRIVATE ${MEALKIT_MODULE_SOURCES_DIRS})
# Every engine module is in this library: MEALKIT_HAS_<NAME>=1 (docs/ENGINE_MODULES.md "Module selection"). Selections below set the left-out ones to 0
set(mealkit_has_defs "")
foreach(m ${MEALKIT_ENGINE_MODULE_NAMES})
    string(TOUPPER ${m} upper)
    string(REPLACE "-" "_" upper ${upper})
    list(APPEND mealkit_has_defs MEALKIT_HAS_${upper}=1)
endforeach()
target_compile_definitions(${mealkit_core_target} PUBLIC ${mealkit_has_defs})

# Dependency order of the modules for core/Modules.h: each module after its requires (and transitional), ties by name (Kahn's algorithm)
set(mealkit_module_pending "")
set(mealkit_kernel_modules "")
foreach(manifest ${mealkit_module_manifests})
    file(STRINGS ${manifest} lines)
    set(m_name "")
    set(m_version "")
    set(m_kind "")
    set(m_deps "")
    foreach(line ${lines})
        if(line MATCHES "^name (.+)$")
            set(m_name ${CMAKE_MATCH_1})
        elseif(line MATCHES "^version (.+)$")
            set(m_version ${CMAKE_MATCH_1})
        elseif(line MATCHES "^kind (.+)$")
            set(m_kind ${CMAKE_MATCH_1})
        elseif(line MATCHES "^(requires|transitional|optional) (.+)$")
            string(REPLACE " " ";" words "${CMAKE_MATCH_2}")
            foreach(w ${words})
                if(NOT w MATCHES "^[0-9.]+$")
                    list(APPEND m_deps ${w})
                endif()
            endforeach()
        endif()
    endforeach()
    list(APPEND mealkit_module_pending ${m_name})
    set(mealkit_module_info_${m_name} "{\"${m_name}\", \"${m_version}\", \"${m_kind}\"},")
    set(mealkit_module_deps_${m_name} ${m_deps})
    if(m_kind STREQUAL "kernel")
        list(APPEND mealkit_kernel_modules ${m_name})
    endif()
endforeach()
list(SORT mealkit_module_pending)
set(mealkit_module_order "")
# Kernel first (it's implied for every module and its members include each other)
foreach(k ${mealkit_kernel_modules})
    list(APPEND mealkit_module_order ${k})
    list(REMOVE_ITEM mealkit_module_pending ${k})
endforeach()
while(mealkit_module_pending)
    set(picked "")
    foreach(m ${mealkit_module_pending})
        set(ready ON)
        foreach(d ${mealkit_module_deps_${m}})
            if(NOT d IN_LIST mealkit_module_order)
                set(ready OFF)
            endif()
        endforeach()
        if(ready)
            set(picked ${m})
            break()
        endif()
    endforeach()
    if(NOT picked)
        message(FATAL_ERROR "engine modules have a requires cycle among: ${mealkit_module_pending} (tools\check_modules.ps1 shows it)")
    endif()
    list(APPEND mealkit_module_order ${picked})
    list(REMOVE_ITEM mealkit_module_pending ${picked})
endwhile()
set(mealkit_module_lines "")
foreach(m ${mealkit_module_order})
    string(APPEND mealkit_module_lines "${mealkit_module_info_${m}}
")
endforeach()
file(CONFIGURE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/generated_modules/mealkit_engine_modules.inc CONTENT "${mealkit_module_lines}")
target_include_directories(${mealkit_core_target} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated_modules)
if(TARGET mealkit AND NOT mealkit_core_target STREQUAL "mealkit")
    target_include_directories(mealkit PRIVATE ${MEALKIT_MODULE_SOURCES_DIRS})
endif()
target_include_directories(${mealkit_core_target} SYSTEM PRIVATE ${MEALKIT_THIRD_PARTY}/cgltf ${MEALKIT_THIRD_PARTY}/stb)
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
    # Game UI: the mkui runtime (engine/libs/mkui, no engine headers; ROADMAP "UI 1") is wrapped by mealkit/ui/ (glue layer).
    # mealkit/ui/Ui.h uses mkui value types (Action, ScreenInfo ...), so only the include path is passed on.
    set(MKUI_STB_DIR ${MEALKIT_THIRD_PARTY}/stb)
    add_subdirectory(libs/mkui ${CMAKE_CURRENT_BINARY_DIR}/mkui)
    target_link_libraries(mealkit PRIVATE mkui)
    target_include_directories(mealkit PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/libs/mkui/include>)
    # Disable warnings for external code implementation files
    target_include_directories(mealkit SYSTEM PRIVATE ${MEALKIT_THIRD_PARTY}/stb
                               ${MEALKIT_THIRD_PARTY}/renderdoc # renderdoc_app.h: rhi/GpuCapture.cpp
                               ${MEALKIT_THIRD_PARTY}/bc7enc)   # asset/TextureCache.cpp
    if(MSVC)
        set_source_files_properties(../modules/ui/src/mealkit/ui/StbImpl.cpp
            ${MEALKIT_THIRD_PARTY}/bc7enc/bc7enc.cpp ${MEALKIT_THIRD_PARTY}/bc7enc/rgbcx.cpp PROPERTIES COMPILE_OPTIONS "/W0")
    endif()
endif()

# Asset import (asset/Cooked.h): glTF reading (cgltf) and mesh optimization (meshoptimizer) only in builds that load source assets:
# dev builds, the editor, headless. The shipping exe (Export, no debug UI) reads cooked assets from the pack only, so those aren't linked into it
if(MEALKIT_DEBUG_UI OR MEALKIT_HEADLESS_ONLY)
    target_compile_definitions(${mealkit_core_target} PUBLIC MEALKIT_ASSET_IMPORT=1)
else()
    target_compile_definitions(${mealkit_core_target} PUBLIC MEALKIT_ASSET_IMPORT=0)
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
# Headless builds have no mealkit target (only mealkit_core)
if(TARGET mealkit)
    if(MEALKIT_LIVE)
        target_compile_definitions(mealkit PUBLIC MEALKIT_LIVE=1)
    else()
        target_compile_definitions(mealkit PUBLIC MEALKIT_LIVE=0)
    endif()
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
            set_source_files_properties(../modules/rhi/src/mealkit/rhi/vulkan/VkLibs.cpp PROPERTIES COMPILE_OPTIONS "/W0")
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
        target_compile_definitions(mealkit_tests PRIVATE MEALKIT_TEST_UI=1 MEALKIT_TEST_FRONT=1)
    elseif(MEALKIT_BUILD_KIND STREQUAL "Editor")
        target_link_libraries(mealkit_tests PRIVATE mkui) # mealkit.dll exports only engine objects; the tests call mkui (Source.h) directly
        target_compile_definitions(mealkit_tests PRIVATE MEALKIT_TEST_UI=1 MEALKIT_TEST_FRONT=1)
    endif()
endif()


# --- Module selections (docs/ENGINE_MODULES.md "Module selection and versions") ---
# The libraries above have every module at the locked version. A project's other selection (cmake/MealKit.cmake mealkit_project_engine)
# is the same library built again: without the left-out modules' sources and with MEALKIT_HAS_<NAME>=0, and with pinned modules taken
# from their fetched folder (the same file list; .cpp files new at the top of that version's src/mealkit/<name>/ are added, missing ones
# dropped). mealkit_core_<key> / mealkit_<key>, made only for selections a project in this build uses, at the end of configuration.
function(mealkit_engine_variant base key)
    set(variant ${base}_${key})
    if(TARGET ${variant})
        return()
    endif()
    get_property(without GLOBAL PROPERTY MEALKIT_SEL_${key}_WITHOUT)
    get_property(overrides GLOBAL PROPERTY MEALKIT_SEL_${key}_OVERRIDES)
    get_target_property(sources ${base} SOURCES)
    set(kept "")
    foreach(src ${sources})
        set(drop OFF)
        foreach(m ${without})
            if(src MATCHES "/modules/${m}/")
                set(drop ON)
            endif()
        endforeach()
        if(NOT drop)
            list(APPEND kept ${src})
        endif()
    endforeach()
    foreach(o ${overrides})
        string(REPLACE "=" ";" parts "${o}")
        list(GET parts 0 m)
        list(GET parts 1 dir)
        set(mapped "")
        foreach(src ${kept})
            if(src MATCHES "/modules/${m}/(.+)$")
                set(file "${dir}/${CMAKE_MATCH_1}")
                if(EXISTS "${file}")
                    list(APPEND mapped "${file}")
                endif()
            else()
                list(APPEND mapped "${src}")
            endif()
        endforeach()
        # Files the engine lists for this module anywhere (both libraries), and which library takes new ones: the core when it has the module
        get_target_property(core_sources mealkit_core SOURCES)
        set(all_sources ${core_sources})
        if(TARGET mealkit) # a file the front library lists is not new for the core either
            get_target_property(front_sources mealkit SOURCES)
            list(APPEND all_sources ${front_sources})
        endif()
        set(core_has OFF)
        foreach(src ${core_sources})
            if(src MATCHES "/modules/${m}/")
                set(core_has ON)
            endif()
        endforeach()
        set(takes_new OFF)
        if((base STREQUAL "mealkit_core" AND core_has) OR (NOT base STREQUAL "mealkit_core" AND NOT core_has))
            set(takes_new ON)
        endif()
        file(GLOB top "${dir}/src/mealkit/${m}/*.cpp")
        foreach(f ${top})
            get_filename_component(fname "${f}" NAME)
            set(listed OFF)
            foreach(src ${all_sources})
                if(src MATCHES "/modules/${m}/src/mealkit/${m}/${fname}$")
                    set(listed ON)
                endif()
            endforeach()
            if(NOT listed AND takes_new)
                list(APPEND mapped "${f}")
            endif()
        endforeach()
        set(kept ${mapped})
    endforeach()
    add_library(${variant} STATIC ${kept})
    foreach(prop INCLUDE_DIRECTORIES COMPILE_DEFINITIONS COMPILE_OPTIONS COMPILE_FEATURES LINK_LIBRARIES PRECOMPILE_HEADERS
                 INTERFACE_INCLUDE_DIRECTORIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES INTERFACE_COMPILE_DEFINITIONS
                 INTERFACE_COMPILE_OPTIONS INTERFACE_LINK_LIBRARIES INTERFACE_LINK_OPTIONS MSVC_RUNTIME_LIBRARY CXX_STANDARD)
        get_target_property(value ${base} ${prop})
        if(value)
            foreach(m ${without})
                string(TOUPPER ${m} upper)
                string(REPLACE "-" "_" upper ${upper})
                string(REPLACE "MEALKIT_HAS_${upper}=1" "MEALKIT_HAS_${upper}=0" value "${value}")
            endforeach()
            foreach(o ${overrides})
                string(REPLACE "=" ";" parts "${o}")
                list(GET parts 0 m)
                list(GET parts 1 dir)
                string(REPLACE "${MEALKIT_ENGINE_DIR}/modules/${m}/" "${dir}/" value "${value}")
            endforeach()
            list(TRANSFORM value REPLACE "^mealkit_core$" "mealkit_core_${key}")
            list(TRANSFORM value REPLACE "^[$]<LINK_ONLY:mealkit_core>$" "$<LINK_ONLY:mealkit_core_${key}>")
            set_property(TARGET ${variant} PROPERTY ${prop} "${value}")
        endif()
    endforeach()
endfunction()
if(NOT MEALKIT_BUILD_KIND STREQUAL "Editor")
    foreach(key ${MEALKIT_VARIANTS_USED})
        mealkit_engine_variant(mealkit_core ${key})
    endforeach()
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
    target_sources(mealkit PRIVATE ../modules/online-steam/src/mealkit/online/OnlineSteam.cpp)
    target_include_directories(mealkit SYSTEM PRIVATE ${MEALKIT_STEAMWORKS_SDK}/public)
    target_link_libraries(mealkit PUBLIC ${MEALKIT_STEAMWORKS_SDK}/redistributable_bin/win64/steam_api64.lib)
    target_compile_definitions(mealkit PUBLIC MEALKIT_STEAM=1)
    # Copied next to the game executable (mealkit_add_game). steam_appid.txt is also made there.
    set(MEALKIT_STEAM_DLL ${MEALKIT_STEAMWORKS_SDK}/redistributable_bin/win64/steam_api64.dll CACHE INTERNAL "")
elseif(mealkit_online STREQUAL "EOS")
    message(STATUS "MealKit: online backend EOS - EOS lobbies and P2P relay enabled")
    # credentials.txt ("name: value" lines) ??EosCredentials.h in the build folder. Never goes into the repo
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
    configure_file(../modules/online-eos/src/mealkit/online/EosCredentials.h.in ${CMAKE_CURRENT_BINARY_DIR}/generated/EosCredentials.h @ONLY)
    target_sources(mealkit PRIVATE ../modules/online-eos/src/mealkit/online/OnlineEOS.cpp)
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
    target_sources(mealkit PRIVATE ../modules/online/src/mealkit/online/OnlineNone.cpp)
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
    # External code is compiled separately from engine warnings/options (/W4, UNICODE) (OBJECT library ??only object files into the engine target)
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

if(NOT MEALKIT_BUILD_KIND STREQUAL "Editor")
    foreach(key ${MEALKIT_VARIANTS_USED})
        mealkit_engine_variant(mealkit ${key})
    endforeach()
endif()

# --- General-purpose editor (Editor build): MealKitEditor.exe = mealkit.dll (+ Dear ImGui + ImGuizmo). Not in the shipping exe ---
if(MEALKIT_BUILD_KIND STREQUAL "Editor")
    add_library(mealkit_editor STATIC
        ../modules/editor/src/mealkit/editor/ImGuiLayer.h
        ../modules/editor/src/mealkit/editor/Editor.h
        ../modules/editor/src/mealkit/editor/Editor.cpp
        ../modules/editor/src/mealkit/editor/EditorData.cpp
        ../modules/editor/src/mealkit/editor/TestsPanel.cpp
        ../modules/editor/src/mealkit/editor/BalancePanel.cpp
        ../modules/editor/src/mealkit/editor/SharedBuild.cpp
        ../modules/editor/src/mealkit/editor/AssetThumbnails.cpp
        ../modules/editor/src/mealkit/editor/Project.h
        ../modules/editor/src/mealkit/editor/Project.cpp
        ../modules/editor/src/mealkit/editor/ProjectManager.h
        ../modules/editor/src/mealkit/editor/ProjectManager.cpp
        ../modules/editor/src/mealkit/editor/GameModuleHost.h
        ../modules/editor/src/mealkit/editor/GameModuleHost.cpp
        ../modules/editor/src/mealkit/editor/FieldEditor.h
        ../modules/editor/src/mealkit/editor/FieldEditor.cpp
        ../modules/editor/src/mealkit/editor/PlaySession.h
        ../modules/editor/src/mealkit/editor/PlaySession.cpp
        ../modules/data/editor/mealkit/editor/TablesPanel.h
        ../modules/data/editor/mealkit/editor/TablesPanel.cpp
        ../modules/audio/editor/mealkit/editor/SoundPanel.h
        ../modules/audio/editor/mealkit/editor/SoundPanel.cpp
        ../modules/feedback/editor/mealkit/editor/FeedbackPanel.h
        ../modules/feedback/editor/mealkit/editor/FeedbackPanel.cpp
        ../modules/ui/editor/mealkit/editor/UiPanel.h
        ../modules/ui/editor/mealkit/editor/UiPanel.cpp
        ../modules/ui/editor/mealkit/editor/UiPanelEdit.cpp
        ../modules/ui/editor/mealkit/editor/UiPanelDrag.cpp
        ../modules/render/editor/mealkit/editor/LookdevPanel.h
        ../modules/render/editor/mealkit/editor/LookdevPanel.cpp
        ../modules/editor/src/mealkit/editor/ImportServer.h
        ../modules/editor/src/mealkit/editor/ImportServer.cpp
        ../modules/editor/src/mealkit/editor/Tools.h
        ../modules/editor/src/mealkit/editor/Tools.cpp
        ../modules/ui/editor/mealkit/editor/UiCheck.h
        ../modules/ui/editor/mealkit/editor/UiCheck.cpp
        ../modules/editor/src/mealkit/editor/TuningPanel.h
        ../modules/editor/src/mealkit/editor/TuningPanel.cpp
        ../modules/editor/src/mealkit/editor/ProjectSettingsPanel.h
        ../modules/editor/src/mealkit/editor/ProjectSettingsPanel.cpp
        ../modules/editor/src/mealkit/editor/BuildPanel.h
        ../modules/editor/src/mealkit/editor/BuildPanel.cpp
        ../modules/editor/src/mealkit/editor/ModulesPanel.cpp
        ../modules/editor/src/mealkit/editor/LogPanel.h
        ../modules/editor/src/mealkit/editor/LogPanel.cpp
        ../modules/editor/src/mealkit/editor/EditorCommands.cpp
        ../modules/editor/src/mealkit/editor/EditorAssets.cpp
        ../modules/editor/src/mealkit/editor/EditorTerrain.cpp
        ../modules/editor/src/mealkit/editor/Preferences.h
        ../modules/editor/src/mealkit/editor/Preferences.cpp
        ../modules/editor/src/mealkit/editor/PerfPanel.h
        ../modules/editor/src/mealkit/editor/PerfPanel.cpp
        ../modules/editor/src/mealkit/editor/CodeLink.h
        ../modules/editor/src/mealkit/editor/CodeLink.cpp
        ../modules/editor/src/mealkit/editor/Strings.h
        ../modules/editor/src/mealkit/editor/Strings.cpp)
    target_link_libraries(mealkit_editor PUBLIC mealkit PRIVATE mkcontract mkui) # mealkit.dll exports only its own objects, not mkcontract's or mkui's (UI editing: mkui/Source.h)
    target_compile_definitions(mealkit_editor PUBLIC MEALKIT_EDITOR=1)
    target_include_directories(mealkit_editor PRIVATE ${MEALKIT_MODULE_SOURCES_DIRS} ${MEALKIT_MODULE_EDITOR_DIRS})

    add_executable(MealKitEditor ../modules/editor/src/mealkit/editor/EditorMain.cpp res/MealKitEditor.rc)
    target_link_libraries(MealKitEditor PRIVATE mealkit_editor)
    target_include_directories(MealKitEditor PRIVATE ${MEALKIT_MODULE_SOURCES_DIRS} ${MEALKIT_MODULE_EDITOR_DIRS})
    # GUI program: no console window behind it (run from a command prompt, attachParentConsole attaches)
    target_link_options(MealKitEditor PRIVATE /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup)
    # Engine folder used by the project manager's new project creation (the editor isn't distributed)
    target_compile_definitions(MealKitEditor PRIVATE MEALKIT_ENGINE_DIR="${MEALKIT_ENGINE_DIR}")

    # Included by a game project's editor build: mealkit.dll import library, header paths, compile options, SimCheck
    export(TARGETS mealkit mealkit_tracy mealkit_simcheck mealkit_stringcheck FILE ${CMAKE_BINARY_DIR}/MealKitTargets.cmake)
endif()

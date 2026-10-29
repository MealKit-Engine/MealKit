# Shader compiler: an official DXC release with a pinned version (docs/DECISIONS.md 2026-10-19 "Vulkan tooling for the RHI layer").
# The Windows SDK dxc can't produce SPIR-V, so it isn't used. Downloaded once, hash-verified, and extracted to a cache outside git:
#   %LOCALAPPDATA%\MealKit\tools\dxc-<version>\bin\x64\dxc.exe
# To use a different dxc: -DMEALKIT_SHADER_DXC=<path> (must be a build with SPIR-V support).
set(MEALKIT_DXC_VERSION v1.9.2609)
set(MEALKIT_DXC_URL https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.9.2609/dxc_2026_09_29.zip)
set(MEALKIT_DXC_SHA256 ad31b1fc8443175d204f77a611fdb3ef2ec42759bdc2f1167368de24a4a7e7f1)

if(NOT MEALKIT_SHADER_DXC)
    if(DEFINED ENV{LOCALAPPDATA})
        file(TO_CMAKE_PATH "$ENV{LOCALAPPDATA}/MealKit/tools" mealkit_tool_cache)
    else()
        set(mealkit_tool_cache "${CMAKE_BINARY_DIR}/_tools")
    endif()
    set(mealkit_dxc_dir "${mealkit_tool_cache}/dxc-${MEALKIT_DXC_VERSION}")
    set(mealkit_dxc_exe "${mealkit_dxc_dir}/bin/x64/dxc.exe")
    if(NOT EXISTS "${mealkit_dxc_exe}")
        set(zip "${mealkit_tool_cache}/dxc-${MEALKIT_DXC_VERSION}.zip")
        message(STATUS "Downloading DXC ${MEALKIT_DXC_VERSION} (shader compiler, once) to ${mealkit_dxc_dir}")
        file(DOWNLOAD "${MEALKIT_DXC_URL}" "${zip}" EXPECTED_HASH SHA256=${MEALKIT_DXC_SHA256} STATUS status TLS_VERIFY ON)
        list(GET status 0 code)
        if(NOT code EQUAL 0)
            file(REMOVE "${zip}")
            message(FATAL_ERROR "DXC download failed (${status}). Download ${MEALKIT_DXC_URL} by hand, unzip it to "
                                "${mealkit_dxc_dir} or pass -DMEALKIT_SHADER_DXC=<dxc.exe with SPIR-V support>.")
        endif()
        file(ARCHIVE_EXTRACT INPUT "${zip}" DESTINATION "${mealkit_dxc_dir}")
        file(REMOVE "${zip}")
    endif()
    set(MEALKIT_SHADER_DXC "${mealkit_dxc_exe}" CACHE FILEPATH "DXC with SPIR-V support (engine shaders)")
endif()

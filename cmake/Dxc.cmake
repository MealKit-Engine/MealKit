# 셰이더 컴파일러: 버전을 고정한 DXC 공식 릴리스 (docs/DECISIONS.md 2026-10-19 "RHI 층의 Vulkan 도구").
# Windows SDK의 dxc는 SPIR-V를 만들지 못하므로 쓰지 않는다. 처음 한 번 받아 해시를 확인하고 git 밖 캐시에 푼다:
#   %LOCALAPPDATA%\MealKit\tools\dxc-<버전>\bin\x64\dxc.exe
# 다른 dxc를 쓰려면 -DMEALKIT_SHADER_DXC=<경로> (SPIR-V 지원 빌드여야 한다).
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

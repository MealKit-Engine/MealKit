@echo off
rem Shared MealKit build script for the engine repository and game projects.
rem   build_project.bat <project folder> [Release|Debug|Profile|Export|Editor]
rem     Release   shipping game in <project>\build\Release\bin (engine linked statically)
rem     Debug     Debug (D3D12 debug layer on)
rem     Profile   Release + Tracy profiling (connect with the Tracy profiler app)
rem     Export    Release game only, collected into <project>\export\<name>
rem     ClangCL   Release built with clang-cl in <project>\build\ClangCL\bin (determinism comparison with MSVC, not shipped)
rem     Vulkan    Release game with the Vulkan RHI backend in <project>\build\Vulkan\bin (backend comparison, not shipped on Windows)
rem     VulkanEditor  editor build with the Vulkan backend in <project>\build\VulkanEditor\bin (like Editor)
rem     Editor    editor build in <project>\build\Editor\bin:
rem                 engine repository: mealkit.dll + MealKitEditor.exe + sample game DLL
rem                 game project:      <name>_game.dll only (builds the engine editor first if needed)
setlocal
if "%~1"=="" (
    echo usage: build_project.bat ^<project folder^> [Release^|Debug^|Profile^|Export^|Editor]
    exit /b 1
)
set PROJECT_DIR=%~f1
for %%i in ("%~dp0..") do set MEALKIT_ROOT=%%~fi

set CONFIG=%~2
if "%CONFIG%"=="" set CONFIG=Release
set BUILD_TYPE=%CONFIG%
set BUILD_NAME=%CONFIG%
set EXTRA=
set TARGET=
if /I "%CONFIG%"=="Profile" (
    set BUILD_TYPE=Release
    set EXTRA=-DMEALKIT_PROFILE=ON
)
if /I "%CONFIG%"=="Export" (
    set BUILD_TYPE=Release
    set BUILD_NAME=Export
    set EXTRA=-DMEALKIT_DEBUG_UI=OFF
    set TARGET=--target game_export
)
if /I "%CONFIG%"=="ClangCL" set BUILD_TYPE=Release
if /I "%CONFIG%"=="Vulkan" (
    set BUILD_TYPE=Release
    set EXTRA=-DMEALKIT_RHI=Vulkan
)
if /I "%CONFIG%"=="VulkanEditor" (
    set BUILD_TYPE=RelWithDebInfo
    set EXTRA=-DMEALKIT_BUILD_KIND=Editor -DMEALKIT_RHI=Vulkan
)
if /I "%CONFIG%"=="Editor" (
    set BUILD_TYPE=RelWithDebInfo
    set BUILD_NAME=Editor
    set EXTRA=-DMEALKIT_BUILD_KIND=Editor
)

if not defined VSCMD_VER (
    call :vcvars || exit /b 1
)
if /I "%CONFIG%"=="ClangCL" (
    call :clangcl || exit /b 1
)

rem A game project's editor build links against the engine editor build (mealkit.dll). Bring it up to date first.
set EDITOR_KIND=
if /I "%CONFIG%"=="Editor" set EDITOR_KIND=1
if /I "%CONFIG%"=="VulkanEditor" set EDITOR_KIND=1
if defined EDITOR_KIND if /I not "%PROJECT_DIR%"=="%MEALKIT_ROOT%" (
    call "%~f0" "%MEALKIT_ROOT%" %CONFIG% || (
        echo [MealKit] Engine editor build failed. If MealKitEditor is running and the engine changed, close it and build again.
        exit /b 1
    )
)

set BUILD_DIR=%PROJECT_DIR%\build\%BUILD_NAME%
rem Editor builds are rebuilt often (hot reload): configure only once, Ninja re-runs CMake when CMakeLists change.
if defined EDITOR_KIND if exist "%BUILD_DIR%\build.ninja" if exist "%BUILD_DIR%\CMakeCache.txt" goto build
cmake -S "%PROJECT_DIR%" -B "%BUILD_DIR%" -G Ninja --no-warn-unused-cli -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DCMAKE_EXPORT_COMPILE_COMMANDS=ON "-DMEALKIT_ROOT=%MEALKIT_ROOT%" %EXTRA% || exit /b 1
:build
cmake --build "%BUILD_DIR%" %TARGET% || exit /b 1
if /I "%CONFIG%"=="Export" (
    echo [MealKit] Export OK: %PROJECT_DIR%\export
) else (
    echo [MealKit] Build OK: %BUILD_DIR%\bin
)
exit /b 0

:vcvars
set VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe
if not exist "%VSWHERE%" (
    echo [MealKit] vswhere.exe not found. Install Visual Studio 2022 or Build Tools with C++.
    exit /b 1
)
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set VSDIR=%%i
if not defined VSDIR (
    echo [MealKit] MSVC x64 tools not found.
    exit /b 1
)
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>nul || exit /b 1
exit /b 0

:clangcl
rem clang-cl: PATH, then any Visual Studio (including previews) with the "C++ Clang tools" component.
set CLANG_CL=
for /f "usebackq tokens=*" %%i in (`where clang-cl 2^>nul`) do if not defined CLANG_CL set CLANG_CL=%%i
if not defined CLANG_CL (
    for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -all -prerelease -products * -requires Microsoft.VisualStudio.Component.VC.Llvm.Clang -property installationPath`) do (
        if not defined CLANG_CL if exist "%%i\VC\Tools\Llvm\x64\bin\clang-cl.exe" set "CLANG_CL=%%i\VC\Tools\Llvm\x64\bin\clang-cl.exe"
    )
)
if not defined CLANG_CL (
    echo [MealKit] clang-cl not found. Install the "C++ Clang tools for Windows" Visual Studio component.
    exit /b 1
)
set EXTRA=%EXTRA% "-DCMAKE_C_COMPILER=%CLANG_CL%" "-DCMAKE_CXX_COMPILER=%CLANG_CL%"
exit /b 0

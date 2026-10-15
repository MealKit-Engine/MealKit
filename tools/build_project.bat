@echo off
rem Shared MealKit build script for the engine repository and game projects.
rem   build_project.bat <project folder> [Release|Debug|Profile|Export|Editor]
rem     Release   shipping game in <project>\build\Release\bin (engine linked statically)
rem     Debug     Debug (D3D12 debug layer on)
rem     Profile   Release + Tracy profiling (connect with the Tracy profiler app)
rem     Export    Release game only, collected into <project>\export\<name>
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
    set BUILD_NAME=Release
    set TARGET=--target game_export
)
if /I "%CONFIG%"=="Editor" (
    set BUILD_TYPE=RelWithDebInfo
    set BUILD_NAME=Editor
    set EXTRA=-DMEALKIT_BUILD_KIND=Editor
)

if not defined VSCMD_VER (
    call :vcvars || exit /b 1
)

rem A game project's editor build links against the engine editor build (mealkit.dll). Bring it up to date first.
if /I "%CONFIG%"=="Editor" if /I not "%PROJECT_DIR%"=="%MEALKIT_ROOT%" (
    call "%~f0" "%MEALKIT_ROOT%" Editor || (
        echo [MealKit] Engine editor build failed. If MealKitEditor is running and the engine changed, close it and build again.
        exit /b 1
    )
)

set BUILD_DIR=%PROJECT_DIR%\build\%BUILD_NAME%
rem Editor builds are rebuilt often (hot reload): configure only once, Ninja re-runs CMake when CMakeLists change.
if /I "%CONFIG%"=="Editor" if exist "%BUILD_DIR%\build.ninja" goto build
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

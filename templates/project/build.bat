@echo off
rem @NAME@ build (MealKit engine project). The engine folder is the "engine" line in mealkit.project.
rem   build.bat Editor     game code DLL for the editor (hot reload; the editor's Ctrl+R does the same)
rem   build.bat            Release: shipping game build\Release\bin\@NAME@.exe
rem   build.bat Debug      Debug (D3D12 debug layer on)
rem   build.bat Profile    Release + Tracy profiling
rem   build.bat Export     release game in export\@NAME@ (same as the editor's Ctrl+B)
setlocal
set MEALKIT_ROOT=
for /f "usebackq eol=# tokens=1,* delims== " %%a in ("%~dp0mealkit.project") do if /I "%%a"=="engine" set "MEALKIT_ROOT=%%b"
if not exist "%MEALKIT_ROOT%\tools\build_project.bat" (
    echo [MealKit] Engine not found at "%MEALKIT_ROOT%" - set the engine line in mealkit.project
    exit /b 1
)
call "%MEALKIT_ROOT%\tools\build_project.bat" "%~dp0." %*
exit /b %errorlevel%

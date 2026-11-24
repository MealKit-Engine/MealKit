@echo off
rem Opens the MealKit editor. Builds the editor first when it is missing or older than the engine sources.
rem Usage: editor.bat                 MealKit Manager (the project list; it must be installed)
rem        editor.bat sample [scene]  the engine's sample project (samples\sandbox), optionally another scene
setlocal
set EXE=%~dp0build\windows\Editor\bin\MealKitEditor.exe
rem Rebuild when the editor is missing or any engine source is newer than it (the build itself is incremental); set MEALKIT_NO_REBUILD=1 to skip
set STALE=
if not exist "%EXE%" (set STALE=1) else if not defined MEALKIT_NO_REBUILD (
    for /f %%i in ('powershell -NoProfile -Command "$e=(Get-Item '%EXE%').LastWriteTime; if (Get-ChildItem -Recurse -File -Path '%~dp0engine','%~dp0modules','%~dp0cmake','%~dp0third_party','%~dp0CMakeLists.txt','%~dp0VERSION' | Where-Object { $_.LastWriteTime -gt $e } | Select-Object -First 1) { 1 }"') do set STALE=1
)
if defined STALE (
    call "%~dp0build.bat" Editor || (pause & exit /b 1)
)
if /i "%~1"=="sample" (
    shift
    goto sample
)
start "" "%EXE%"
exit /b 0

:sample
start "" "%EXE%" --project "%~dp0samples\sandbox" %1 %2 %3 %4 %5 %6 %7 %8 %9

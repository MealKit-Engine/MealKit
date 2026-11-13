@echo off
rem Opens the MealKit editor. Builds the editor first if needed.
rem Usage: editor.bat                 MealKit Manager (the project list; it must be installed)
rem        editor.bat sample [scene]  the engine's sample project (samples\sandbox), optionally another scene
setlocal
set EXE=%~dp0build\Editor\bin\MealKitEditor.exe
if not exist "%EXE%" (
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

@echo off
rem Opens the MealKit editor on the engine's sample project (samples\sandbox). Builds the editor first if needed.
rem Without a project the editor shows the project manager: build\Editor\bin\MealKitEditor.exe
rem Usage: editor.bat [scene file]
setlocal
set EXE=%~dp0build\Editor\bin\MealKitEditor.exe
if not exist "%EXE%" (
    call "%~dp0build.bat" Editor || (pause & exit /b 1)
)
start "" "%EXE%" --project "%~dp0." %*

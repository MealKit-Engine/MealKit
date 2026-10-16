@echo off
rem Opens this project in the MealKit editor (MealKitEditor.exe from the engine folder).
rem The editor builds the game code itself if it is missing or older than the engine.
rem Usage: editor.bat [scene file]
setlocal
set MEALKIT_ROOT=
for /f "usebackq eol=# tokens=1,* delims== " %%a in ("%~dp0mealkit.project") do if /I "%%a"=="engine" set "MEALKIT_ROOT=%%b"
set EXE=%MEALKIT_ROOT%\build\Editor\bin\MealKitEditor.exe
if not exist "%EXE%" (
    call "%~dp0build.bat" Editor || (pause & exit /b 1)
)
start "" "%EXE%" --project "%~dp0." %*

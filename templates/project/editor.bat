@echo off
rem Opens this project in the Aster editor (AsterEditor.exe from the engine folder).
rem The editor builds the game code itself if it is missing or older than the engine.
rem Usage: editor.bat [scene file]
setlocal
set ASTER_ROOT=
for /f "usebackq eol=# tokens=1,* delims==" %%a in ("%~dp0aster.project") do if /I "%%a"=="engine" set "ASTER_ROOT=%%b"
set EXE=%ASTER_ROOT%\build\Editor\bin\AsterEditor.exe
if not exist "%EXE%" (
    call "%~dp0build.bat" Editor || (pause & exit /b 1)
)
start "" "%EXE%" --project "%~dp0." %*

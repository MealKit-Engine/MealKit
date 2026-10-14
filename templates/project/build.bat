@echo off
rem @NAME@ build (Aster engine project). The engine folder is engine= in aster.project.
rem   build.bat Editor     game code DLL for the editor (hot reload; the editor's Ctrl+R does the same)
rem   build.bat            Release: shipping game build\Release\bin\@NAME@.exe
rem   build.bat Debug      Debug (D3D12 debug layer on)
rem   build.bat Profile    Release + Tracy profiling
rem   build.bat Export     release game in export\@NAME@ (same as the editor's Ctrl+B)
setlocal
set ASTER_ROOT=
for /f "usebackq eol=# tokens=1,* delims==" %%a in ("%~dp0aster.project") do if /I "%%a"=="engine" set "ASTER_ROOT=%%b"
if not exist "%ASTER_ROOT%\tools\build_project.bat" (
    echo [Aster] Engine not found at "%ASTER_ROOT%" - set engine= in aster.project
    exit /b 1
)
call "%ASTER_ROOT%\tools\build_project.bat" "%~dp0." %*
exit /b %errorlevel%

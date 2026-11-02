@echo off
rem Windows entry for the shared build script tools\build.ps1 (configurations are listed there).
rem   build_project.bat <project folder> [Release|Debug|Profile|Export|Editor|...]
setlocal
if "%~1"=="" (
    echo usage: build_project.bat ^<project folder^> [Release^|Debug^|Profile^|Export^|Editor^|EOS^|ClangCL^|Vulkan^|VulkanEditor]
    exit /b 1
)
set CONFIG=%~2
if "%CONFIG%"=="" set CONFIG=Release
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0build.ps1" -Project "%~f1" -Config %CONFIG%
exit /b %errorlevel%

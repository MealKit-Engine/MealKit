@echo off
rem MealKit engine repository build: engine + sample game (samples\sandbox).
rem Game projects have their own build.bat (tools\new_project.ps1).
rem   build.bat            Release
rem   build.bat Debug      Debug (D3D12 debug layer on)
rem   build.bat Profile    Release + Tracy profiling (connect with the Tracy profiler app)
rem   build.bat Export     reference samples only, collected into samples\<name>\export\windows\<name>
rem   build.bat EOS        Release with the EOS online backend (build\windows\EOS, needs third_party\eos)
rem   build.bat Vulkan     Release with the Vulkan RHI backend (build\windows\Vulkan), VulkanEditor: editor with Vulkan (build\windows\VulkanEditor)
rem Build trees are build\<os>\<config> (tools\build_dirs.ps1).
call "%~dp0tools\build_project.bat" "%~dp0." %*
exit /b %errorlevel%

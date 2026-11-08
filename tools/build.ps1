# Shared MealKit build script for the engine repository and game projects, on every OS (Windows PowerShell 5.1 and PowerShell 7).
# Windows calls it through build.bat -> tools\build_project.bat; Linux/macOS run it with pwsh (build.ps1 in the project folder).
#   build.ps1 -Project <project folder> [-Config Release|Debug|Profile|Export|Editor|...]
#     Release   shipping game in <project>/build/Release/bin (engine linked statically)
#     Debug     Debug (D3D12 debug layer on)
#     Profile   Release + Tracy profiling (connect with the Tracy profiler app)
#     Export    Release game only, collected into <project>/export/<name>
#     HeadlessOnly  Windows: the Linux/macOS CI build (core + headless targets only, clang-cl) in <project>/build/HeadlessOnly/bin
#     ClangCL   Windows: Release built with clang-cl in <project>/build/ClangCL/bin (determinism comparison with MSVC, not shipped)
#     Vulkan    Release game with the Vulkan RHI backend in <project>/build/Vulkan/bin (backend comparison, not shipped on Windows)
#     VulkanEditor  editor build with the Vulkan backend in <project>/build/VulkanEditor/bin (like Editor)
#     Presets   Release plus every engine preset (engine repository: also <sample>_solo.exe built without net, docs/ENGINE_MODULES.md "Presets")
#     EOS       Release with the EOS online backend (build/EOS, needs third_party/eos)
#     Headless  Linux/macOS until their window/editor exist: headless game + tests in <project>/build/Headless/bin
#     Editor    editor build in <project>/build/Editor/bin:
#                 engine repository: mealkit.dll + MealKitEditor.exe + sample game DLL
#                 game project:      <name>_game.dll only (builds the engine editor first if needed)
# Windows uses MSVC (vcvars64 from vswhere); Linux/macOS use clang/clang++ unless CC/CXX are set. Generator is always Ninja.
param(
    [Parameter(Mandatory = $true)][string]$Project,
    [string]$Config = "Release",
    [string[]]$Target = @() # cmake --build --target (default: everything, Export: game_export)
)
$ErrorActionPreference = "Stop"
$onWindows = [Environment]::OSVersion.Platform -eq [PlatformID]::Win32NT
$projectDir = (Resolve-Path $Project).Path.TrimEnd("\", "/")
$engineDir = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path.TrimEnd("\", "/")

function Fail([string]$message) {
    Write-Host "[MealKit] $message"
    exit 1
}
# Runs a native command and stops on failure (PowerShell 5.1 doesn't stop on native exit codes)
function Invoke-Native([string]$exe, [string[]]$arguments) {
    & $exe @arguments
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
# Runs this script again for another configuration (engine editor build, Release for cooking)
function Invoke-Self([string]$dir, [string]$config, [string]$failMessage) {
    $shell = (Get-Process -Id $PID).Path # the same PowerShell (5.1 or 7)
    & $shell -NoProfile -ExecutionPolicy Bypass -File $PSCommandPath -Project $dir -Config $config
    if ($LASTEXITCODE -ne 0) { Fail $failMessage }
}

$buildType = $Config
$buildName = $Config
$extra = @()
$isEditor = $false
switch ($Config) {
    "Release" {}
    "Debug" {}
    "Profile" { $buildType = "Release"; $extra = @("-DMEALKIT_PROFILE=ON") }
    "Export" {
        $buildType = "Release"
        $extra = @("-DMEALKIT_DEBUG_UI=OFF", "-DMEALKIT_LIVE=OFF")
        if ($Target.Count -eq 0) { $Target = @("game_export") }
    }
    "ClangCL" { $buildType = "Release" }
    "Vulkan" { $buildType = "Release"; $extra = @("-DMEALKIT_RHI=Vulkan") }
    "EOS" { $buildType = "Release"; $extra = @("-DMEALKIT_ONLINE=EOS") }
    "Presets" { $buildType = "Release"; $extra = @("-DMEALKIT_EXTRA_PRESETS=solo") }
    "Headless" { $buildType = "Release" }
    "HeadlessOnly" { $buildType = "Release"; $extra = @("-DMEALKIT_FORCE_HEADLESS_ONLY=ON") }
    "VulkanEditor" { $buildType = "RelWithDebInfo"; $extra = @("-DMEALKIT_BUILD_KIND=Editor", "-DMEALKIT_RHI=Vulkan"); $isEditor = $true }
    "Editor" { $buildType = "RelWithDebInfo"; $extra = @("-DMEALKIT_BUILD_KIND=Editor"); $isEditor = $true }
    default { Fail "unknown configuration '$Config' (Release, Debug, Profile, Export, Editor, ClangCL, Vulkan, VulkanEditor, EOS, Presets, Headless, HeadlessOnly)" }
}

if ($onWindows) {
    # MSVC environment (vcvars64) unless this shell already has one
    if (-not $env:VSCMD_VER) {
        $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
        if (-not (Test-Path $vswhere)) { Fail "vswhere.exe not found. Install Visual Studio 2022 or Build Tools with C++." }
        $vsDir = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if (-not $vsDir) { Fail "MSVC x64 tools not found." }
        $vcvars = Join-Path $vsDir "VC\Auxiliary\Build\vcvars64.bat"
        foreach ($line in (cmd /c "`"$vcvars`" >nul 2>nul && set")) {
            $eq = $line.IndexOf("=")
            if ($eq -gt 0) { [Environment]::SetEnvironmentVariable($line.Substring(0, $eq), $line.Substring($eq + 1), "Process") }
        }
        if (-not $env:VSCMD_VER) { Fail "vcvars64.bat failed: $vcvars" }
    }
    if ($Config -eq "ClangCL" -or $Config -eq "HeadlessOnly") {
        # clang-cl: PATH, then any Visual Studio (including previews) with the "C++ Clang tools" component
        $clang = (Get-Command clang-cl -ErrorAction SilentlyContinue | Select-Object -First 1).Source
        if (-not $clang) {
            $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
            foreach ($dir in (& $vswhere -all -prerelease -products * -requires Microsoft.VisualStudio.Component.VC.Llvm.Clang -property installationPath)) {
                $candidate = Join-Path $dir "VC\Tools\Llvm\x64\bin\clang-cl.exe"
                if (-not $clang -and (Test-Path $candidate)) { $clang = $candidate }
            }
        }
        if (-not $clang) { Fail "clang-cl not found. Install the `"C++ Clang tools for Windows`" Visual Studio component." }
        $extra += @("-DCMAKE_C_COMPILER=$clang", "-DCMAKE_CXX_COMPILER=$clang")
    }
} else {
    if ($Config -eq "ClangCL") { Fail "ClangCL is a Windows configuration (clang is already the compiler here)" }
    if (-not $env:CC) { $extra += "-DCMAKE_C_COMPILER=clang" }
    if (-not $env:CXX) { $extra += "-DCMAKE_CXX_COMPILER=clang++" }
}

# A game project's editor build links against the engine editor build (mealkit.dll). Bring it up to date first.
if ($isEditor -and $projectDir -ne $engineDir) {
    Invoke-Self $engineDir $Config "Engine editor build failed. If MealKitEditor is running and the engine changed, close it and build again."
}
# Export cooks the assets with the development Release exe (game/Cook.h), so bring that up to date first
if ($Config -eq "Export") { Invoke-Self $projectDir "Release" "The Release build used for cooking failed." }

$buildDir = Join-Path (Join-Path $projectDir "build") $buildName
# Editor builds are rebuilt often (hot reload): configure only once, Ninja re-runs CMake when CMakeLists change.
$configured = (Test-Path (Join-Path $buildDir "build.ninja")) -and (Test-Path (Join-Path $buildDir "CMakeCache.txt"))
if (-not ($isEditor -and $configured)) {
    Invoke-Native "cmake" (@("-S", $projectDir, "-B", $buildDir, "-G", "Ninja", "--no-warn-unused-cli", "-DCMAKE_BUILD_TYPE=$buildType",
            "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON", "-DMEALKIT_ROOT=$engineDir") + $extra)
}
$buildArgs = @("--build", $buildDir)
if ($Target.Count) { $buildArgs += @("--target") + $Target }
Invoke-Native "cmake" $buildArgs
if ($Config -eq "Export") {
    Write-Host "[MealKit] Export OK: $(Join-Path $projectDir 'export')"
} else {
    Write-Host "[MealKit] Build OK: $(Join-Path $buildDir 'bin')"
}
exit 0

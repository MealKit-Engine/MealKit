# Shared MealKit build script for the engine repository and game projects, on every OS (Windows PowerShell 5.1 and PowerShell 7).
# Windows calls it through build.bat -> tools\build_project.bat; Linux/macOS run it with pwsh (build.ps1 in the project folder).
#   build.ps1 -Project <project folder> [-Config Release|Debug|Profile|Export|Editor|...]
#   Build trees are <project>/build/<os>/<Config> (os = windows, linux, macos; tools/build_dirs.ps1), exports <project>/export/<os>/<name>.
#     Release   shipping game in <project>/build/<os>/Release/bin (engine linked statically)
#     Debug     Debug (D3D12 debug layer on)
#     Profile   Release + Tracy profiling (connect with the Tracy profiler app)
#     Export    Release game only, collected into <project>/export/<os>/<name>
#     HeadlessOnly  Windows: the Linux/macOS CI build (core + headless targets only, clang-cl) in <project>/build/<os>/HeadlessOnly/bin
#     ClangCL   Windows: Release built with clang-cl in <project>/build/<os>/ClangCL/bin (determinism comparison with MSVC, not shipped)
#     Vulkan    Release game with the Vulkan RHI backend in <project>/build/<os>/Vulkan/bin (backend comparison, not shipped on Windows)
#     VulkanEditor  editor build with the Vulkan backend in <project>/build/<os>/VulkanEditor/bin (like Editor)
#     VulkanExport  Export with the Vulkan backend (Linux: the same as Export)
#     profile:<name>  the configuration (and for an export the output folder) written in <project>/profiles/<name>.profile (docs/DECISIONS.md 2026-11-23 "Build profiles")
#     Presets   Release plus every engine preset (engine repository: also <sample>_solo.exe without net and <sample>_noaudio.exe without audio, docs/ENGINE_MODULES.md "Presets")
#     EOS       Release with the EOS online backend (build/<os>/EOS, needs third_party/eos)
#     Headless  Linux/macOS: headless game + tests in <project>/build/<os>/Headless/bin
#     Linux: every configuration but Headless builds the X11 front end with the Vulkan backend (Editor = VulkanEditor, Release = the Linux game exe)
#     Editor    editor build in <project>/build/<os>/Editor/bin:
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
. (Join-Path $PSScriptRoot "build_dirs.ps1")
. (Join-Path $PSScriptRoot "build_deps.ps1") # code page and header-dependency safety, so every caller of this script is protected (docs/CHANGELOG.md "Build traps")

function Fail([string]$message) {
    Write-Host "[MealKit] $message"
    Restore-BuildCodePage
    exit 1
}
# Runs a native command and stops on failure (PowerShell 5.1 doesn't stop on native exit codes)
function Invoke-Native([string]$exe, [string[]]$arguments) {
    & $exe @arguments
    if ($LASTEXITCODE -ne 0) { Restore-BuildCodePage; exit $LASTEXITCODE }
}
# Runs this script again for another configuration (engine editor build, Release for cooking)
function Invoke-Self([string]$dir, [string]$config, [string]$failMessage) {
    $shell = (Get-Process -Id $PID).Path # the same PowerShell (5.1 or 7)
    & $shell -NoProfile -ExecutionPolicy Bypass -File $PSCommandPath -Project $dir -Config $config
    if ($LASTEXITCODE -ne 0) { Fail $failMessage }
}

# A build profile (profiles/<name>.profile, "config", "output" and "platform" lines) names the configuration and the folder an export goes to
$exportOut = ""
if ($Config -like "profile:*") {
    $profileName = $Config.Substring(8)
    $profileFile = Join-Path (Join-Path $projectDir "profiles") "$profileName.profile"
    if ($profileName -notmatch '^[A-Za-z0-9_.-]+$' -or -not (Test-Path $profileFile)) { Fail "build profile '$profileName' not found: $profileFile" }
    $Config = ""
    $profilePlatform = ""
    foreach ($line in Get-Content $profileFile -Encoding UTF8) {
        if ($line -match '^\s*config\s*[= ]\s*(\S+)\s*$') { $Config = $Matches[1] }
        elseif ($line -match '^\s*output\s*[= ]\s*(.+?)\s*$') { $exportOut = $Matches[1] }
        elseif ($line -match '^\s*platform\s*[= ]\s*(\S+)\s*$') { $profilePlatform = $Matches[1] }
    }
    # No cross builds: a profile for another OS stops here instead of building this OS's exe into its folder
    if ($profilePlatform) {
        $hostOs = if ($onWindows) { "windows" } elseif ($IsMacOS) { "macos" } else { "linux" }
        if ($profilePlatform -ne $hostOs) { Fail "build profile '$profileName' builds on $profilePlatform (this PC: $hostOs)" }
    }
    if (-not $Config -or $Config -like "profile:*") { Fail "build profile '$profileName' has no valid config line" }
    if ($exportOut -and $Config -notin @("Export", "VulkanExport")) { Fail "build profile '$profileName': output is only for Export and VulkanExport" }
    if ($exportOut) {
        if (-not [IO.Path]::IsPathRooted($exportOut)) { $exportOut = Join-Path $projectDir $exportOut }
        $exportOut = $exportOut.Replace("\", "/")
    }
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
    "VulkanExport" {
        $buildType = "Release"
        $extra = @("-DMEALKIT_RHI=Vulkan", "-DMEALKIT_DEBUG_UI=OFF", "-DMEALKIT_LIVE=OFF")
        if ($Target.Count -eq 0) { $Target = @("game_export") }
    }
    "ClangCL" { $buildType = "Release" }
    "Vulkan" { $buildType = "Release"; $extra = @("-DMEALKIT_RHI=Vulkan") }
    "EOS" { $buildType = "Release"; $extra = @("-DMEALKIT_ONLINE=EOS") }
    "Presets" { $buildType = "Release"; $extra = @("-DMEALKIT_EXTRA_PRESETS=solo;standard/audio") }
    "Headless" { $buildType = "Release" }
    "HeadlessOnly" { $buildType = "Release"; $extra = @("-DMEALKIT_FORCE_HEADLESS_ONLY=ON") }
    "VulkanEditor" { $buildType = "RelWithDebInfo"; $extra = @("-DMEALKIT_BUILD_KIND=Editor", "-DMEALKIT_RHI=Vulkan"); $isEditor = $true }
    "WebGPU" { $buildType = "Release"; $extra = @("-DMEALKIT_RHI=WebGPU") } # Linux: Dawn (MkRender's WebGPU backend)
    "WebGPUEditor" { $buildType = "RelWithDebInfo"; $extra = @("-DMEALKIT_BUILD_KIND=Editor", "-DMEALKIT_RHI=WebGPU"); $isEditor = $true }
    "Editor" { $buildType = "RelWithDebInfo"; $extra = @("-DMEALKIT_BUILD_KIND=Editor"); $isEditor = $true }
    default { Fail "unknown configuration '$Config' (Release, Debug, Profile, Export, VulkanExport, profile:<name>, Editor, ClangCL, Vulkan, VulkanEditor, WebGPU, WebGPUEditor, EOS, Presets, Headless, HeadlessOnly)" }
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
    if ($Config -notin @("Headless", "HeadlessOnly")) { # every other configuration is a front end build here (Release, Export ... = the Linux game exe)
        if (-not $IsLinux) { Fail "$Config needs a front end, which exists on Windows and Linux only (macOS: ROADMAP stage 5)" }
        $extra += "-DMEALKIT_LINUX_FRONTEND=ON"
    }
    if (-not $env:CC) { $extra += "-DCMAKE_C_COMPILER=clang" }
    if (-not $env:CXX) { $extra += "-DCMAKE_CXX_COMPILER=clang++" }
}

# The console's code page must be the one CMake detects msvc_deps_prefix in (Windows only; a UTF-8 console loses every header dependency)
Set-BuildCodePage

# The engine repository builds its reference samples: fetch the ones samples.lock pins and samples/ lacks (tools\sample.ps1 sync), so a
# first build after cloning never meets a missing folder. A sample that is there is left alone (tools\sample.ps1 status compares it).
$sampleLock = Join-Path $engineDir "samples.lock"
if ($projectDir -eq $engineDir -and (Test-Path $sampleLock)) {
    $missingSamples = @(Get-Content $sampleLock | Where-Object { $_ -match '^sample\s+(\S+)' } | ForEach-Object { $Matches[1] } |
        Where-Object { -not (Test-Path (Join-Path (Join-Path $engineDir "samples") $_)) })
    if ($missingSamples.Count -gt 0) {
        Write-Host "[MealKit] Fetching the locked samples: $($missingSamples -join ', ')"
        $shell = (Get-Process -Id $PID).Path
        & $shell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot "sample.ps1") sync @missingSamples
        if ($LASTEXITCODE -ne 0) { Fail "could not fetch the samples (tools\sample.ps1 sync)" }
    }
}

# A game project's editor build links against the engine editor build (mealkit.dll). Bring it up to date first.
# An installed engine (MealKit Manager, tools\package_engine.ps1 writes INSTALLED) ships that build ready-made and isn't rebuilt.
$installedEngine = Test-Path (Join-Path $engineDir "INSTALLED")
if ($isEditor -and $installedEngine -and $Config -ne "Editor") { Fail "An installed engine ships only the Editor build; $Config needs the engine repository." }
if ($isEditor -and $projectDir -ne $engineDir -and -not $installedEngine) {
    Invoke-Self $engineDir $Config "Engine editor build failed. If MealKitEditor is running and the engine changed, close it and build again."
}
# Export cooks the assets with the development Release exe (game/Cook.h), so bring that up to date first
if ($Config -in @("Export", "VulkanExport")) { Invoke-Self $projectDir "Release" "The Release build used for cooking failed." }

$buildDir = Get-NewBuildDir $projectDir $buildName
# A tree of the layout before build/<os>/ (build/<Config>) can't be moved (CMake keeps its absolute path): it's built anew once
$oldDir = Join-Path (Join-Path $projectDir "build") $buildName
if (-not (Test-Path $buildDir) -and (Test-Path (Join-Path $oldDir "CMakeCache.txt"))) {
    Write-Host "[MealKit] Building $buildName into $buildDir (build trees are per OS now). The old tree $oldDir can be deleted once this build works."
}
# Editor builds are rebuilt often (hot reload): configure only once, Ninja re-runs CMake when CMakeLists change.
$configured = (Test-Path (Join-Path $buildDir "build.ninja")) -and (Test-Path (Join-Path $buildDir "CMakeCache.txt"))
if (-not ($isEditor -and $configured)) {
    # Always set for an export (empty clears the value a previous profile left in the CMake cache)
    if ($Config -in @("Export", "VulkanExport")) { $extra += "-DMEALKIT_EXPORT_DIR=$exportOut" }
    Invoke-Native "cmake" (@("-S", $projectDir, "-B", $buildDir, "-G", "Ninja", "--no-warn-unused-cli", "-DCMAKE_BUILD_TYPE=$buildType",
            "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON", "-DMEALKIT_ROOT=$engineDir") + $extra)
}
if (-not (Test-DepsPrefix $buildDir)) {
    Fail "the compiler output doesn't match msvc_deps_prefix in ${buildDir}: a build now would lose header dependencies. Delete CMakeFiles\rules.ninja and CMakeCache.txt there to reconfigure."
}
$buildArgs = @("--build", $buildDir)
if ($Target.Count) { $buildArgs += @("--target") + $Target }
Invoke-Native "cmake" $buildArgs
# Objects that lost their header dependencies (stale after a header or class layout change) are deleted and rebuilt once
$zeroDeps = @(Get-ZeroDepObjects $buildDir)
if ($zeroDeps.Count) {
    Write-Host "[MealKit] $($zeroDeps.Count) object(s) have no recorded header dependencies; deleting them and rebuilding:"
    foreach ($obj in $zeroDeps) {
        Write-Host "  $obj"
        Remove-Item -Force (Join-Path $buildDir $obj) -ErrorAction SilentlyContinue
    }
    Invoke-Native "cmake" $buildArgs
    $zeroDeps = @(Get-ZeroDepObjects $buildDir)
    if ($zeroDeps.Count) { Fail "still no header dependencies after rebuilding: $($zeroDeps -join ', ')" }
}
Restore-BuildCodePage
# clangd and VS Code read build/compile_commands.json (.clangd, .vscode/settings.json): the Editor tree of this OS, which has every source
if ($Config -eq "Editor") {
    $commands = Join-Path $buildDir "compile_commands.json"
    if (Test-Path $commands) { Copy-Item $commands (Join-Path (Join-Path $projectDir "build") "compile_commands.json") -Force }
}
if ($Config -in @("Export", "VulkanExport")) {
    Write-Host "[MealKit] Export OK: $(if ($exportOut) { $exportOut } else { Join-Path (Join-Path $projectDir 'export') $MealKitBuildOs })"
} else {
    Write-Host "[MealKit] Build OK: $(Join-Path $buildDir 'bin')"
}
exit 0

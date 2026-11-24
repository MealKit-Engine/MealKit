# Build folder layout, shared by the build script and the tools that run what it builds (dot-source: . "$PSScriptRoot/build_dirs.ps1").
# Build trees are <root>/build/<os>/<Config> (os = windows, linux, macos) and exports <project>/export/<os>/<name> (docs/DECISIONS.md 2026-11-24
# "Build trees per OS"). Folders that aren't CMake trees (build/editor, build/live, build/cache, build/check ...) stay directly in build/.
# Trees from before that layout (build/<Config>) are still read when no new tree exists, so an old engine or project keeps working until rebuilt.
$MealKitBuildOs = if ([Environment]::OSVersion.Platform -eq [PlatformID]::Win32NT) { "windows" } elseif ($IsMacOS) { "macos" } else { "linux" }

# Where a build tree is written: <root>/build/<os>/<config>
function Get-NewBuildDir([string]$root, [string]$config) {
    return Join-Path (Join-Path (Join-Path $root "build") $MealKitBuildOs) $config
}

# Where an existing build tree is read: the new tree, else an old build/<config> tree, else the new path (not built yet)
function Get-BuildDir([string]$root, [string]$config) {
    $new = Get-NewBuildDir $root $config
    $old = Join-Path (Join-Path $root "build") $config
    if (-not (Test-Path $new) -and (Test-OwnOsTree $old)) { return $old }
    return $new
}

# Was this tree configured on this OS? (an old build/<config> may come from Windows on a shared drive: absolute C:/ compiler path)
function Test-OwnOsTree([string]$dir) {
    $cache = Join-Path $dir "CMakeCache.txt"
    if (-not (Test-Path $cache)) { return $false }
    $windowsTree = Select-String -Path $cache -Pattern '^CMAKE_CXX_COMPILER:[A-Z]+=[A-Za-z]:/' -Quiet
    return [bool]$windowsTree -eq ($MealKitBuildOs -eq "windows")
}

# The build/ folder a built file or folder sits in (<root>/build/<os>/<Config>/bin/x or an old <root>/build/<Config>/bin/x), "" if none
function Get-BuildFolderOf([string]$path) {
    $dir = Split-Path -Parent ([IO.Path]::GetFullPath($path))
    while ($dir) {
        if ((Split-Path -Leaf $dir) -eq "build") { return $dir }
        $parent = Split-Path -Parent $dir
        if ($parent -eq $dir) { break }
        $dir = $parent
    }
    return ""
}

# A build tree's bin folder (see Get-BuildDir)
function Get-BuildBin([string]$root, [string]$config) { return Join-Path (Get-BuildDir $root $config) "bin" }

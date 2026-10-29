# Tool suite runtime boundary check (docs/DECISIONS.md 2026-10-26 audio runtime). Called by check.ps1.
#   powershell -ExecutionPolicy Bypass -File tools\check_libs.ps1 [-Path <folder>]     exit code 1 on problems
# Targets: third_party/<lib>/ whose mealkit.txt source is a MealKit-Engine repo (runtime pieces we made, currently mkaudio),
# and the folder given with -Path (a local checkout being edited together, MEALKIT_MKAUDIO_DIR). This code lives in repos outside the engine, so:
# - doesn't include MealKit headers (#include "mealkit/..." / <mealkit/...>) or engine/src paths
# - doesn't use MEALKIT_ macros (logging/profiling/asserts)
# - doesn't include outside its own folder via relative paths ("../")
param([string[]]$Path = @())
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot

$dirs = @()
foreach ($d in Get-ChildItem -Path (Join-Path $root "third_party") -Directory) {
    $meta = Join-Path $d.FullName "mealkit.txt"
    if ((Test-Path $meta) -and (Select-String -Path $meta -Pattern '^source https://github\.com/MealKit-Engine/' -Quiet)) { $dirs += $d }
}
foreach ($p in $Path) { $dirs += Get-Item $p }

$problems = @()
$count = 0
foreach ($lib in $dirs) {
    $count++
    foreach ($f in Get-ChildItem -Path $lib.FullName -Recurse -Include *.h, *.hpp, *.c, *.cpp, *.inl, CMakeLists.txt) {
        $rel = $f.FullName.Substring($root.Length + 1)
        $n = 0
        foreach ($line in [IO.File]::ReadAllLines($f.FullName)) {
            $n++
            $code = ($line -replace '//.*$', '') -replace '#\s.*$', '' # comments (CMake comments are after "# ")
            if ($f.Name -eq "CMakeLists.txt") {
                if ($code -match 'MEALKIT_|engine/src|mealkit_') { $problems += "${rel}:${n}: uses the engine build ($($Matches[0]))" }
                continue
            }
            $code = ($line -replace '//.*$', '')
            if ($code -match '^\s*#\s*include\s*["<]([^">]+)[">]') {
                $inc = $Matches[1]
                if ($inc -match '^mealkit/' -or $inc -match 'engine/src') { $problems += "${rel}:${n}: includes $inc (engine header)" }
                elseif ($inc -match '^\.\./\.\./|^\.\./(?!src/|include/)') { $problems += "${rel}:${n}: includes $inc (outside the library)" }
            }
            if ($code -match '\bMEALKIT_\w+') { $problems += "${rel}:${n}: uses $($Matches[0]) (engine macro)" }
        }
    }
}

if ($problems.Count -gt 0) {
    $problems | ForEach-Object { Write-Output "error: $_" }
    Write-Output "FAIL libs: $($problems.Count) problem(s) - tool runtimes must not depend on the engine (docs/DECISIONS.md 2026-10-26)"
    exit 1
}
Write-Output "OK libs: $count library folder(s) do not use engine headers or macros"
exit 0

# Public header check (docs/DECISIONS.md 2026-11-03 "Contract freeze scope"). Called by check.ps1.
#   powershell -ExecutionPolicy Bypass -File tools\check_public_headers.ps1 [-Path <folder>]     exit code 1 on problems
# Game code includes only the engine headers listed in engine\public_headers.txt (the ones v1.x keeps source compatible).
# Targets: the reference samples' game code and source modules (samples\<name>\sim, view, app, modules), the project templates
# (templates\), and the folders given with -Path (a game project's source\ or a module checkout).
# Also fails when the list names a header that doesn't exist (renamed or removed: that's a v2.0 change).
param([string[]]$Path = @())
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
. "$PSScriptRoot\engine_modules_common.ps1"
$listFile = Join-Path $root "engine\public_headers.txt"

$public = @{}
$problems = @()
foreach ($line in [IO.File]::ReadAllLines($listFile)) {
    $h = ($line -replace '#.*$', '').Trim()
    if (-not $h) { continue }
    $public[$h] = $true
    if (-not (Resolve-EngineHeader $root $h)) { $problems += "engine\public_headers.txt: mealkit/$h isn't in that module's include/ folder" }
}

$dirs = @()
foreach ($sample in Get-ChildItem -Path (Join-Path $root "samples") -Directory) {
    foreach ($sub in "sim", "view", "app", "modules") {
        $d = Join-Path $sample.FullName $sub
        if (Test-Path $d) { $dirs += $d }
    }
}
$dirs += Join-Path $root "templates"
foreach ($p in $Path) { $dirs += (Resolve-Path $p).Path }

$count = 0
foreach ($dir in $dirs) {
    foreach ($f in Get-ChildItem -Path $dir -Recurse -Include *.h, *.hpp, *.cpp, *.inl) {
        if ($f.FullName -match '\\(build|export|third_party)\\') { continue }
        $count++
        $rel = if ($f.FullName.StartsWith($root)) { $f.FullName.Substring($root.Length + 1) } else { $f.FullName }
        $n = 0
        foreach ($line in [IO.File]::ReadAllLines($f.FullName)) {
            $n++
            if ($line -match '^\s*#\s*include\s*["<]mealkit/([^">]+)[">]' -and -not $public.ContainsKey($Matches[1])) {
                $problems += "${rel}:${n}: includes mealkit/$($Matches[1]), which isn't a public header (engine\public_headers.txt)"
            }
        }
    }
}

if ($problems.Count -gt 0) {
    $problems | ForEach-Object { Write-Output "error: $_" }
    Write-Output "FAIL public headers: $($problems.Count) problem(s) - game code uses only engine\public_headers.txt (add a header there if games need it)"
    exit 1
}
Write-Output "OK public headers: $count game code file(s) include only the $($public.Count) public engine headers"
exit 0

# Engine folder layer check (docs/CODING_MODEL.md "Dependencies between engine folders"). Called by check.ps1.
#   powershell -ExecutionPolicy Bypass -File tools\check_layers.ps1     exit code 1 on problems
# Files in engine/src/mealkit/<folder> may only #include "mealkit/<folder>/..." from folders earlier in (or in the same slot of) the order below.
# Fails if a folder not in the order appears: give new folders a place here and in CODING_MODEL.md.
# platform/<os>/ implementation files implement headers from several folders (net/Socket.h, core/Clock.h etc.), so they're excluded and check_platform.ps1 covers them.
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$engineSrc = Join-Path $root "engine\src\mealkit"

# Front to back. Multiple folders in one slot may include each other (only the base core/platform)
$layers = @(
    @("core", "platform"), # base
    @("net"), @("physics"), @("sim"), @("data"), # simulation side (used by headless)
    @("live"), # dev tool connection (live connection server, tool contract)
    @("loc"), @("scene"), @("rhi"), @("render"), @("asset"), @("online"), @("debug"), # view side
    @("input"), @("audio"), @("ui"), @("feedback"), @("settings"),
    @("game"), # game boundary (GameInstance, GameContext)
    @("editor")
)
$rank = @{}
for ($i = 0; $i -lt $layers.Count; $i++) { foreach ($d in $layers[$i]) { $rank[$d] = $i } }

$problems = @()
foreach ($dir in Get-ChildItem -Path $engineSrc -Directory) {
    if (-not $rank.ContainsKey($dir.Name)) { $problems += "engine\src\mealkit\$($dir.Name): folder is not in the layer order (tools\check_layers.ps1)" }
}

$files = Get-ChildItem -Path $engineSrc -Recurse -Include *.cpp, *.h |
    Where-Object { $_.FullName -notmatch '\\mealkit\\platform\\(win32|posix)\\' }
$edges = 0
foreach ($f in $files) {
    $rel = $f.FullName.Substring($root.Length + 1)
    $from = $f.FullName.Substring($engineSrc.Length + 1).Split('\')[0]
    if (-not $rank.ContainsKey($from)) { continue } # already reported above
    $n = 0
    foreach ($line in [IO.File]::ReadAllLines($f.FullName)) {
        $n++
        $code = ($line -replace '//.*$', '')
        if ($code -notmatch '^\s*#\s*include\s*["<]mealkit/(\w+)/') { continue }
        $to = $Matches[1]
        if ($to -eq $from) { continue }
        $edges++
        if (-not $rank.ContainsKey($to)) { $problems += "${rel}:${n}: includes mealkit/$to/ which is not in the layer order"; continue }
        if ($rank[$to] -gt $rank[$from]) { $problems += "${rel}:${n}: $from/ includes $to/ (later layer)" }
    }
}

# Core files (MEALKIT_CORE_SOURCES in engine/CMakeLists.txt, linked by headless/Linux/macOS) include only core headers.
# Folder order alone can't catch core → view-side includes within the same folder (game/Headless.cpp → game/Frontend.h etc.).
$cmake = [IO.File]::ReadAllText((Join-Path $root "engine\CMakeLists.txt")) -replace '#[^\r\n]*', '' # so parentheses in comments don't break the list
$core = @{}
foreach ($block in [regex]::Matches($cmake, '(?s)(?:set|list\(APPEND)\s*\(?\s*MEALKIT_CORE_SOURCES\b(.*?)\)')) {
    foreach ($m in [regex]::Matches($block.Groups[1].Value, 'src/(mealkit/[\w/]+\.(?:h|cpp))')) { $core[$m.Groups[1].Value] = $true }
}
$coreEdges = 0
foreach ($f in $files) {
    $key = "mealkit/" + $f.FullName.Substring($engineSrc.Length + 1).Replace('\', '/')
    if (-not $core.ContainsKey($key)) { continue }
    $rel = $f.FullName.Substring($root.Length + 1)
    $n = 0
    foreach ($line in [IO.File]::ReadAllLines($f.FullName)) {
        $n++
        $code = ($line -replace '//.*$', '')
        if ($code -notmatch '^\s*#\s*include\s*["<](mealkit/[\w/]+\.h)[">]') { continue }
        $coreEdges++
        if (-not $core.ContainsKey($Matches[1])) { $problems += "${rel}:${n}: core file includes $($Matches[1]) (not in MEALKIT_CORE_SOURCES)" }
    }
}
if ($core.Count -eq 0) { $problems += "engine\CMakeLists.txt: MEALKIT_CORE_SOURCES not found" }

$order = ($layers | ForEach-Object { $_ -join "+" }) -join " < "
if ($problems.Count -gt 0) {
    $problems | ForEach-Object { Write-Host $_ }
    Write-Host "order: $order"
    Write-Host "FAIL: $($problems.Count) problem(s) with the engine layer order" -ForegroundColor Red
    exit 1
}
Write-Host "OK: $($files.Count) files, $edges cross-folder includes follow $order; $($core.Count) core files include only core headers ($coreEdges includes)" -ForegroundColor Green
exit 0

# MealKit engine verification: prints PASS/FAIL and elapsed time per item, exit code 1 if any fail.
#   powershell -ExecutionPolicy Bypass -File tools\check.ps1            everything (about 5 minutes; only when the user asks for it)
#   ... -Changed           only the groups matching modified files (git working tree, and since that commit with -Base <commit>). The default, also before committing
#   ... -Quick             build + unit tests + determinism tests only
#   ... -Only det,render   only those groups ($allGroups below)
#   ... -DryRun            only shows the groups it would pick and stops (with -Changed)
#   ... -UpdatePerf        rewrites the performance baseline (engine\tests\perf\baseline.txt) with this PC's --benchmark numbers (run on the measuring PC)
#   ... -UpdateGolden      replaces the graphics regression test reference images (engine\tests\golden) with this run's images (when images were changed on purpose)
# Groups: static (check scripts) unit (unit tests) det (determinism/multiplayer) game (game window) tools (tool connections) render (graphics/Vulkan)
#       editor (editor play/hot reload) project (new project/upgrade) export (shipping build)
# Unless it's a full check, the determinism group is skipped when the "determinism key" (simulation version + content the simulation reads) matches the last pass
# (build\check-state\det.key). If the key changed, it runs regardless of the chosen groups.
# Logs and screenshots: build\check\  (<number>-<name>.log per item)
# Close any running editor first (mealkit.dll gets rebuilt).
param(
    [switch]$Quick,
    [switch]$Changed,
    [string]$Base = "",
    [string[]]$Only = @(),
    [switch]$UpdateGolden,
    [switch]$UpdatePerf,
    [switch]$DryRun
)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$out = Join-Path $root "build\check"
New-Item -ItemType Directory -Force $out | Out-Null
Get-ChildItem $out -File | Remove-Item -Force
$stateDir = Join-Path $root "build\check-state"
$detKeyFile = Join-Path $stateDir "det.key"

$allGroups = @("static", "unit", "det", "game", "tools", "render", "editor", "project", "export")

# Groups that run when an engine folder (engine/src/mealkit/<folder>) is modified. core/platform (and folders not in the table) run everything except export.
# Unit tests run only the area with the same name (TEST_CASE(area, ...)). "det!" = the determinism group regardless of the determinism key
$folderGroups = @{
    jobs = "unit det game editor render"; net = "unit det game editor"; physics = "unit det game editor"; sim = "unit det game editor"; data = "unit det game editor"
    scene = "unit det game editor render"; asset = "unit det game render"; game = "unit det! game editor project"
    live = "unit game tools"; loc = "unit game editor"; input = "unit game editor"; settings = "unit game"; ui = "unit game tools editor"
    rhi = "unit game render"; render = "unit game render"; online = "game"; debug = "game editor"; audio = "game"; feedback = "game editor"
    editor = "editor tools render"
}
# Path rules (first match from the top). Values: group name, "folder:<engine folder>", "unit-all", "det!", "full" (everything except export), "" = no checks
$pathRules = @(
    @('\.md$', ''), @('^docs/', ''), @('^\.github/', ''), @('^\.claude/', ''), @('^\.git(ignore|attributes)$', ''),
    @('^art/ui/preview/', 'tools'), @('^art/', ''),
    @('^engine/src/mealkit/([^/]+)/', 'folder:$1'), @('^engine/shaders/', 'folder:render'),
    @('^engine/tests/golden/', 'render'), @('^engine/tests/perf/', 'game'), @('^engine/tests/', 'unit-all'),
    @('^engine/tools/SimCheck', 'editor project'), @('^engine/tools/', 'game'), @('^engine/res/', 'editor'),
    @('^third_party/mkaudio/', 'folder:audio'), @('^third_party/', 'full'),
    @('^samples/[^/]+/(sim|replays|tests)/', 'det! game editor'), @('^samples/sandbox/app/(Flow|Screens)', 'game editor project'),
    @('^samples/', 'game editor'),
    @('^assets/sounds', 'folder:audio'), @('^assets/ui/', 'folder:ui project'), @('^assets/strings/', 'folder:loc project'),
    @('^assets/(game\.tuning|scenes/|models/|tables/)', 'det game editor render'), @('^assets/', 'game editor'),
    @('^mealkit\.project$', 'det game editor'),
    @('^(templates/|VERSION$|tools/(new|upgrade|rename)_project|tools/project_common|tools/module\.ps1$)', 'project'),
    @('^tools/(penpot-plugin|blender-addon)/', 'tools'),
    @('^tools/(mp_test|record_replay|ci_|desync_diff)', 'det!'), @('^tools/(live_probe|compare_images)', 'game render tools'),
    @('^tools/(check_|gen_notices)', 'static'), @('^tools/check\.ps1$', ''), @('^tools/update_mkaudio', 'folder:audio'),
    @('^(cmake/|CMakeLists\.txt$|engine/CMakeLists\.txt$|build\.bat$|editor\.bat$|tools/build_project)', 'full')
)

# Chosen groups: $groups[name] = $true, $unitAreas = $null (all) or a list of areas, $detForced = determinism regardless of the key
$groups = @{}
$unitAreas = $null
$detForced = $false
function Add-All { $allGroups | Where-Object { $_ -ne "export" } | ForEach-Object { $script:groups[$_] = $true } }
if ($Only.Count -gt 0) {
    $mode = "only"
    foreach ($g in ($Only -join ",").Split(",")) {
        if ($allGroups -notcontains $g) { Write-Host "unknown group '$g' (groups: $($allGroups -join ', '))" -ForegroundColor Red; exit 1 }
        $groups[$g] = $true
    }
    $detForced = [bool]$groups["det"]
} elseif ($Changed) {
    $mode = "changed"
    $files = @()
    if ($Base) { $files += @(& git -C $root diff --name-only $Base) }
    $files += @(& git -C $root status --porcelain --untracked-files=all | ForEach-Object { ($_.Substring(3) -replace '^.* -> ', '').Trim('"') })
    $files = @($files | Where-Object { $_ } | Sort-Object -Unique)
    $areas = @{}
    $allUnit = $false
    foreach ($f in $files) {
        $tokens = $null
        foreach ($rule in $pathRules) {
            $m = [regex]::Match($f, $rule[0])
            if ($m.Success) { $tokens = $m.Result($rule[1]); break }
        }
        if ($null -eq $tokens) { Write-Host "  $f -> full (no rule in check.ps1)" -ForegroundColor Yellow; $tokens = "full" }
        foreach ($t in $tokens.Split(" ", [StringSplitOptions]::RemoveEmptyEntries)) {
            if ($t -eq "full") { Add-All; $allUnit = $true; $detForced = $true }
            elseif ($t -eq "unit-all") { $groups["unit"] = $true; $allUnit = $true }
            elseif ($t -eq "det!") { $groups["det"] = $true; $detForced = $true }
            elseif ($t.StartsWith("folder:")) {
                $folder = $t.Substring(7)
                if (-not $folderGroups.ContainsKey($folder)) { Add-All; $allUnit = $true } # core, platform
                else {
                    foreach ($g in $folderGroups[$folder].Split(" ")) {
                        if ($g -eq "det!") { $groups["det"] = $true; $detForced = $true } else { $groups[$g] = $true }
                    }
                    $areas[$folder] = $true
                }
            } else { $groups[$t] = $true }
        }
    }
    # Only areas that have unit tests (first argument of TEST_CASE in engine/tests/Tests.cpp)
    $testAreas = @(Select-String -Path (Join-Path $root "engine\tests\Tests.cpp") -Pattern '^TEST_CASE\((\w+),' | ForEach-Object { $_.Matches[0].Groups[1].Value })
    if ($groups["unit"] -and -not $allUnit) { $unitAreas = @($areas.Keys | Where-Object { $testAreas -contains $_ } | Sort-Object) }
    if ($groups.Count -gt 0) { $groups["static"] = $true }
    Write-Host ("changed: {0} file(s)" -f $files.Count)
} elseif ($Quick) {
    $mode = "quick"
    foreach ($g in "static", "unit", "det") { $groups[$g] = $true }
} else {
    $mode = "full"
    foreach ($g in $allGroups) { $groups[$g] = $true }
    $detForced = $true
}
if ($groups["unit"] -and $null -ne $unitAreas -and $unitAreas.Count -eq 0) { $groups.Remove("unit") }
if ($UpdatePerf) { $groups["game"] = $true }
function Selected([string]$group) { return [bool]$groups[$group] }
$needRelease = (Selected "unit") -or (Selected "det") -or (Selected "game") -or (Selected "project") -or (Selected "export")
$needEditor = (Selected "unit") -or (Selected "tools") -or (Selected "render") -or (Selected "editor")

$release = Join-Path $root "build\Release\bin"
$editorBin = Join-Path $root "build\Editor\bin"
$game = Join-Path $release "sandbox.exe"
$headless = Join-Path $release "sandbox_headless.exe"
$editor = Join-Path $editorBin "MealKitEditor.exe"
$results = @()
$step = 0

# One item: PASS if $body returns $true. Output goes to a log file.
function Invoke-Check([string]$group, [string]$name, [scriptblock]$body) {
    if (-not (Selected $group)) { return }
    $script:step++
    $log = Join-Path $out ("{0:D2}-{1}.log" -f $script:step, ($name -replace '[^A-Za-z0-9]+', '-'))
    $watch = [Diagnostics.Stopwatch]::StartNew()
    $ok = $false
    $detail = ""
    try {
        $r = & $body $log
        if ($r -is [array]) { $r = $r[-1] }
        # "SKIP: reason" = can't be done on this PC, "WARN: reason" = worth a look (neither is a failure)
        if ($r -is [string]) { $detail = $r; $ok = $r.StartsWith("SKIP") -or $r.StartsWith("WARN") }
        else { $ok = [bool]$r }
    } catch {
        $detail = $_.Exception.Message
        Add-Content -Path $log -Value $detail
    }
    $watch.Stop()
    $status = if ($detail.StartsWith("SKIP")) { "SKIP" } elseif ($detail.StartsWith("WARN")) { "WARN" } elseif ($ok) { "PASS" } else { "FAIL" }
    $color = if ($status -eq "SKIP" -or $status -eq "WARN") { "Yellow" } elseif ($ok) { "Green" } else { "Red" }
    Write-Host ("{0,-4} {1,-34} {2,7:N1}s  {3}" -f $status, $name, $watch.Elapsed.TotalSeconds, $detail) -ForegroundColor $color
    $script:results += [pscustomobject]@{ Group = $group; Name = $name; Ok = $ok; Skipped = ($status -eq "SKIP"); Warned = ($status -eq "WARN"); Seconds = $watch.Elapsed.TotalSeconds; Log = $log }
}

# Runs a program and logs stdout/stderr (cmd sends them to the file directly). Returns the exit code.
function Run([string]$log, [string]$exe, [string]$arguments) {
    & cmd.exe /d /s /c "`"`"$exe`" $arguments > `"$log`" 2>&1`""
    return $LASTEXITCODE
}

function Build([string]$log, [string]$kind) {
    $code = Run $log "$root\build.bat" $kind
    return $code -eq 0 -and (Select-String -Path $log -Pattern "Build OK|Export OK" -Quiet)
}

function Has([string]$log, [string]$pattern) { return [bool](Select-String -Path $log -Pattern $pattern -Quiet) }

$picked = @($allGroups | Where-Object { Selected $_ })
$unitText = if ((Selected "unit") -and $null -ne $unitAreas) { " (unit: $($unitAreas -join ','))" } else { "" }
Write-Host "MealKit check ($mode): $(if ($picked.Count) { $picked -join ' ' } else { 'nothing to check' })$unitText - logs in $out"
if ($picked.Count -eq 0) { exit 0 }
if ($DryRun) {
    if ((Selected "det") -and -not $detForced) { Write-Host "det runs only if the simulation key changed since the last pass" }
    exit 0
}
$groups["build-release"] = $needRelease
$groups["build-editor"] = $needEditor

# Determinism key: the Release build's simulation version (cmake/BuildInfo.cmake, hash of sources touching the simulation) + content the simulation reads
function Get-DetKey {
    $parts = @()
    foreach ($h in @(Get-ChildItem (Join-Path $root "build\Release\samples") -Recurse -Filter mealkit_build_info.h -ErrorAction SilentlyContinue | Sort-Object FullName)) {
        $parts += "$($h.Directory.Name) " + (Select-String -Path $h.FullName -Pattern "SIM_VERSION (\w+)").Matches[0].Groups[1].Value
    }
    if ($parts.Count -eq 0) { return "" }
    $content = @(@("mealkit.project", "assets\game.tuning") | ForEach-Object { Join-Path $root $_ } | Where-Object { Test-Path $_ })
    $content += @(Get-ChildItem (Join-Path $root "assets") -Recurse -File -Include *.scene, *.glb, *.gltf, *.table -ErrorAction SilentlyContinue | ForEach-Object FullName)
    $content += @(Get-ChildItem (Join-Path $root "samples") -Recurse -File -Filter *.rec | ForEach-Object FullName)
    $content += @(Get-ChildItem (Join-Path $root "samples") -Recurse -File -Filter *.scenario | ForEach-Object FullName)
    foreach ($f in ($content | Sort-Object)) { $parts += $f.Substring($root.Length + 1) + " " + (Get-FileHash $f -Algorithm SHA256).Hash }
    $sha = [Security.Cryptography.SHA256]::Create()
    return [BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($parts -join "`n"))).Replace("-", "").Substring(0, 16)
}

Invoke-Check build-editor "Editor build" { param($log) Build $log "Editor" }
Invoke-Check build-release "Release build" { param($log) Build $log "" }
$unitArgs = if ($null -ne $unitAreas) { "--only $($unitAreas -join ',')" } else { "" }
$releaseUnitLog = ""
Invoke-Check unit "Unit tests (Release)" { param($log) $script:releaseUnitLog = $log; (Run $log "$release\mealkit_tests.exe" $unitArgs) -eq 0 }
Invoke-Check unit "Unit tests (Editor) + DetMath bits" {
    param($log)
    if ((Run $log "$editorBin\mealkit_tests.exe" $unitArgs) -ne 0) { return $false }
    # detmath-bits is printed by the det_math (core area) test. Not compared if excluded by --only
    $a = @((Select-String -Path $log -Pattern "^detmath-bits (\w+)").Matches)
    $b = if ($releaseUnitLog) { @((Select-String -Path $releaseUnitLog -Pattern "^detmath-bits (\w+)").Matches) } else { @() }
    if ($a.Count -gt 0 -and $b.Count -gt 0 -and $a[0].Groups[1].Value -ne $b[0].Groups[1].Value) {
        return "Release $($b[0].Groups[1].Value) != Editor $($a[0].Groups[1].Value)"
    }
    return $true
}
Invoke-Check static "Editor strings (ko.txt)" {
    param($log)
    $output = & powershell -NoProfile -ExecutionPolicy Bypass -File "$PSScriptRoot\check_strings.ps1" 2>&1
    $output | Out-File -Encoding utf8 $log
    return $LASTEXITCODE -eq 0
}
Invoke-Check static "Project layout + notices" {
    param($log)
    $output = & powershell -NoProfile -ExecutionPolicy Bypass -File "$PSScriptRoot\check_layout.ps1" 2>&1
    $code = $LASTEXITCODE
    $output += & powershell -NoProfile -ExecutionPolicy Bypass -File "$PSScriptRoot\gen_notices.ps1" -Check 2>&1
    $output | Out-File -Encoding utf8 $log
    return $code -eq 0 -and $LASTEXITCODE -eq 0
}
Invoke-Check static "Platform layer (OS APIs)" {
    param($log)
    $output = & powershell -NoProfile -ExecutionPolicy Bypass -File "$PSScriptRoot\check_platform.ps1" 2>&1
    $output | Out-File -Encoding utf8 $log
    return $LASTEXITCODE -eq 0
}
Invoke-Check static "Engine layers (include order)" {
    param($log)
    $output = & powershell -NoProfile -ExecutionPolicy Bypass -File "$PSScriptRoot\check_layers.ps1" 2>&1
    $output | Out-File -Encoding utf8 $log
    return $LASTEXITCODE -eq 0
}
# Tool suite runtimes (third_party/mkaudio = separate repo MealKit-Audio) don't use engine headers/macros
Invoke-Check static "Engine libs (no engine headers)" {
    param($log)
    $output = & powershell -NoProfile -ExecutionPolicy Bypass -File "$PSScriptRoot\check_libs.ps1" 2>&1
    $output | Out-File -Encoding utf8 $log
    return $LASTEXITCODE -eq 0
}
# Determinism group: always for full checks, -Only det, or when modified files require it ("det!"); otherwise only when the key changed. If the key changed, it runs even if not chosen
$detKey = if ($needRelease) { Get-DetKey } else { "" }
$savedKey = if (Test-Path $detKeyFile) { (Get-Content $detKeyFile -Raw).Trim() } else { "" }
if ($detKey -and -not $detForced) {
    if ($detKey -eq $savedKey) {
        if (Selected "det") { Write-Host "SKIP determinism group: simulation key $detKey unchanged since the last pass" -ForegroundColor Yellow }
        $groups["det"] = $false
    } elseif (-not (Selected "det")) {
        Write-Host "simulation key changed ($(if ($savedKey) { $savedKey } else { 'none' }) -> $detKey): adding the determinism group" -ForegroundColor Yellow
        $groups["det"] = $true
    }
}

Invoke-Check det "Determinism (mp_test 1+3, 900 ticks)" {
    param($log)
    $output = & powershell -NoProfile -ExecutionPolicy Bypass -File "$PSScriptRoot\mp_test.ps1" -Clients 3 -Ticks 900 2>&1
    $output | Out-File -Encoding utf8 $log
    return ($output -match "^PASS").Count -gt 0
}

# Headless: the same determinism tests without a window + replaying the input recording with another executable (sandbox.exe --headless) to check per-tick hashes match
Invoke-Check det "Headless (mp_test, record + replay)" {
    param($log)
    $output = & powershell -NoProfile -ExecutionPolicy Bypass -File "$PSScriptRoot\mp_test.ps1" -Clients 3 -Ticks 900 -Headless 2>&1
    $output | Out-File -Encoding utf8 $log
    if (($output -match "^PASS").Count -eq 0) { return "mp_test -Headless failed" }
    $rec = Join-Path $out "headless.rec"
    Run "$log.record" $headless "--seed 7 --exit-tick 900 --record-inputs `"$rec`" --hash-log `"$out\headless-hashes.txt`"" | Out-Null
    $code = Run "$log.replay" $game "--headless --replay-inputs `"$rec`" --hash-log `"$out\replay-hashes.txt`""
    if ($code -ne 0 -or -not (Has "$log.replay" "^REPLAY ok")) { return "replay failed" }
    if ((Get-FileHash "$out\headless-hashes.txt").Hash -ne (Get-FileHash "$out\replay-hashes.txt").Hash) { return "hash logs differ" }
    return $true
}

# Desync diagnostics: when one client deliberately diverges, does everyone detect the desync, and does the dump comparison pinpoint that tick (headless 1+1)
Invoke-Check det "Desync dumps + desync_diff" {
    param($log)
    $output = & powershell -NoProfile -ExecutionPolicy Bypass -File "$PSScriptRoot\mp_test.ps1" -Clients 1 -Ticks 400 -Headless -DesyncTest 200 2>&1
    $output | Out-File -Encoding utf8 $log
    return ($output -match "^PASS").Count -gt 0
}

# Late join/reconnect: do someone who left and rejoined with the same name and someone newly joining mid-match keep the same hash to the end (headless 1+2+1)
Invoke-Check det "Late join + rejoin (mp_test -LateJoin)" {
    param($log)
    $output = & powershell -NoProfile -ExecutionPolicy Bypass -File "$PSScriptRoot\mp_test.ps1" -Clients 2 -Ticks 3000 -Headless -LateJoin 2>&1
    $output | Out-File -Encoding utf8 $log
    return ($output -match "^PASS: 4 peers").Count -gt 0
}

# Host migration: when the host leaves normally (Handoff) and the successor also crashes, do the remaining two clients keep the same hash to the end (headless 1+3)
Invoke-Check det "Host migration (mp_test -HostLeave)" {
    param($log)
    $output = & powershell -NoProfile -ExecutionPolicy Bypass -File "$PSScriptRoot\mp_test.ps1" -Clients 3 -Ticks 3000 -Headless -HostLeave 2>&1
    $output | Out-File -Encoding utf8 $log
    return ($output -match "^PASS: 2 peers").Count -gt 0
}

# Whether the repo's fixed input recordings (samples/*/replays/*.rec, replayed on all OSes by CI) match the current code.
# If simulation results were changed on purpose, regenerate the recordings (docs repo "Engine development > Testing").
Invoke-Check det "Committed replays (samples/*/replays)" {
    param($log)
    $recs = @(Get-ChildItem -Path (Join-Path $root "samples") -Recurse -Filter *.rec | Where-Object { $_.Directory.Name -eq "replays" })
    if ($recs.Count -eq 0) { return "no recordings" }
    foreach ($rec in $recs) {
        $exe = Join-Path $release "$($rec.Directory.Parent.Name)_headless.exe"
        $code = Run "$log.$($rec.BaseName)" $exe "--replay-inputs `"$($rec.FullName)`""
        if ($code -ne 0 -or -not (Has "$log.$($rec.BaseName)" "^REPLAY ok")) { return "$($rec.Name) differs - re-record if intended" }
    }
    return $true
}

# Scenario tests (samples/*/tests/*.scenario, game/Scenario.h): game rules checked on the headless World.
# If results change on purpose: <sample>_headless --test <file> --update-expected
Invoke-Check det "Scenario tests (samples/*/tests)" {
    param($log)
    $files = @(Get-ChildItem -Path (Join-Path $root "samples") -Recurse -Filter *.scenario | Where-Object { $_.Directory.Name -eq "tests" })
    if ($files.Count -eq 0) { return "no scenarios" }
    foreach ($f in $files) {
        $exe = Join-Path $release "$($f.Directory.Parent.Name)_headless.exe"
        $code = Run "$log.$($f.BaseName)" $exe "--test `"$($f.FullName)`""
        if ($code -ne 0 -or -not (Has "$log.$($f.BaseName)" "^SCENARIO ok")) { return "$($f.Name) failed (see $log.$($f.BaseName))" }
    }
    return $true
}

# -Quick skips from here (the windowed 900-tick determinism items; unselected groups are skipped anyway)
if (-not $Quick) {
    Invoke-Check det "Shadow simulation + rewind" {
        param($log)
        Run $log $game "--bot --mute --seed 7 --exit-tick 900 --shadow --rewind-test --vsync 0 --size 640 360 --hash-log `"$out\window-hashes.txt`"" | Out-Null
        return (Has $log "^SHADOW ok") -and (Has $log "^REWIND ok")
    }
    Invoke-Check det "Headless = window (900 tick hashes)" {
        param($log)
        $a = Join-Path $out "headless-hashes.txt"
        $b = Join-Path $out "window-hashes.txt"
        if (-not (Test-Path $a) -or -not (Test-Path $b)) { return "hash log missing" }
        return (Get-FileHash $a).Hash -eq (Get-FileHash $b).Hash
    }
    Invoke-Check game "Game capture" {
        param($log)
        $png = Join-Path $out "game.png"
        $code = Run $log $game "--bot --mute --seed 7 --exit-tick 300 --capture `"$png`" 240"
        return $code -eq 0 -and (Test-Path $png)
    }
    # Performance baseline (ROADMAP "Performance baseline check"): --benchmark on the fixed bot match vs engine\tests\perf\baseline.txt.
    # Warning only (machines differ; a baseline from another GPU is not compared). -UpdatePerf rewrites it on the measuring PC
    Invoke-Check game "Performance baseline" {
        param($log)
        $report = Join-Path $out "benchmark.txt"
        $code = Run $log $game "--benchmark 10 --mute --benchmark-out `"$report`""
        $m = [regex]::Match((Get-Content $log -Raw), 'BENCHMARK frames=\d+ avg=([\d.]+)ms p99=([\d.]+)ms worst=[\d.]+ms sim=([\d.]+)ms gpu=([\d.]+)ms mem=(\d+)MB gpumem=(\d+)MB(?: allocs=([\d.]+)/(\d+))?')
        if ($code -ne 0 -or -not $m.Success -or -not (Test-Path $report)) { return $false }
        $now = [ordered]@{ avg = [double]$m.Groups[1].Value; p99 = [double]$m.Groups[2].Value; sim = [double]$m.Groups[3].Value
            gpu = [double]$m.Groups[4].Value; mem = [double]$m.Groups[5].Value; gpumem = [double]$m.Groups[6].Value }
        # Frame allocations (ROADMAP optimization 9, debug/AllocCount.h): main thread heap allocations per frame in render + UI.
        # The same on every PC, so compared even when the adapter differs; any rise over the baseline (+1 for rounding) warns. The goal is 0
        $allocs = if ($m.Groups[7].Success) { [double]$m.Groups[7].Value } else { -1 }
        $adapter = ((Select-String -Path $report -Pattern '^gpu\s+(.+?) \|').Matches[0].Groups[1].Value).Trim()
        $baseFile = Join-Path $root "engine\tests\perf\baseline.txt"
        if ($UpdatePerf) {
            New-Item -ItemType Directory -Force (Split-Path $baseFile) | Out-Null
            $lines = @("# Performance baseline for tools\check.ps1 (sandbox.exe --benchmark 10, fixed seed + bots). Rewrite with check.ps1 -UpdatePerf on the measuring PC",
                "# Values over baseline * (1 + tolerance %) (and over the absolute slack: 0.5 ms for times, 32 MB for memory) are warnings",
                "adapter $adapter", "tolerance 20")
            foreach ($k in $now.Keys) { $lines += "$k $($now[$k])" }
            if ($allocs -ge 0) { $lines += "allocs $allocs" }
            [IO.File]::WriteAllLines($baseFile, $lines)
            return "WARN: baseline updated ($adapter)"
        }
        if (-not (Test-Path $baseFile)) { return "WARN: no baseline (run check.ps1 -UpdatePerf on the measuring PC)" }
        $base = @{}
        foreach ($line in Get-Content $baseFile) {
            if ($line -match '^\s*(#|$)') { continue }
            $i = $line.IndexOf(" "); $base[$line.Substring(0, $i)] = $line.Substring($i + 1).Trim()
        }
        $over = @()
        if ($allocs -ge 0 -and $base.ContainsKey("allocs")) {
            $ba = [double]$base["allocs"]
            Add-Content -Path $log -Value ("perf {0,-7} {1,9} (baseline {2})" -f "allocs", $allocs, $ba)
            if ($allocs -gt $ba + 1.0) { $over += "allocs $allocs/frame > $ba (render + UI heap allocations, see benchmark.txt)" }
        }
        if ($base["adapter"] -ne $adapter) {
            if ($over.Count -gt 0) { return "WARN: $($over -join ', ')" }
            return "SKIP: baseline is from '$($base["adapter"])', this PC is '$adapter'"
        }
        $tol = [double]$base["tolerance"] / 100.0
        foreach ($k in $now.Keys) {
            if (-not $base.ContainsKey($k)) { continue }
            $b = [double]$base[$k]
            $slack = if ($k -like "*mem") { 32.0 } else { 0.5 }
            $unit = if ($k -like "*mem") { "MB" } else { "ms" }
            Add-Content -Path $log -Value ("perf {0,-7} {1,9} (baseline {2})" -f $k, $now[$k], $b)
            if ($now[$k] -gt [Math]::Max($b * (1 + $tol), $b + $slack)) { $over += "$k $($now[$k])$unit > $b$unit" }
        }
        if ($over.Count -gt 0) { return "WARN: over baseline +$($base["tolerance"])%: $($over -join ', ')" }
        return $true
    }
    # Crash report (ROADMAP "Crash reports"): crash on purpose → crash/pending/ (minidump, info, log) → the next run bundles crash/<time>.zip with replay.rec
    Invoke-Check game "Crash report" {
        param($log)
        $crashDir = Join-Path $env:LOCALAPPDATA "sandbox\crash" # userFolder of the sample's mealkit.project
        $before = @(Get-ChildItem $crashDir -Filter *.zip -ErrorAction SilentlyContinue | ForEach-Object FullName)
        $rec = Join-Path $out "crash.rec"
        $code = Run $log $game "--bot --mute --vsync 0 --exit-tick 900 --crash-at 120 --record-inputs `"$rec`""
        if ($code -eq 0) { return "the game did not crash" }
        if (-not (Test-Path (Join-Path $crashDir "pending\crash.dmp"))) { return "no crash.dmp in crash\pending" }
        $log2 = "$log.next"
        $code = Run $log2 $game "--bot --mute --vsync 0 --exit-tick 30 --crash-at 100000"
        $new = @(Get-ChildItem $crashDir -Filter *.zip | Where-Object { $before -notcontains $_.FullName })
        $ok = $code -eq 0 -and $new.Count -eq 1 -and (Has $log2 "Crash report saved") -and -not (Test-Path (Join-Path $crashDir "pending\crash.txt"))
        $new | Remove-Item # test reports are deleted
        return $ok
    }
    # Screen flow: title → menu → solo → lobby → start → match (presses widgets by name, ends by frame count)
    # GPU capture (ROADMAP "GPU capture button and editor view modes"): if RenderDoc is present, capture one frame with --gpu-capture-at and check an .rdc appears
    Invoke-Check game "GPU capture (RenderDoc)" {
        param($log)
        if (-not (Test-Path "$env:ProgramFiles\RenderDoc\renderdoc.dll")) { return "SKIP: RenderDoc not installed" }
        $dir = Join-Path $env:LOCALAPPDATA "MealKit\gpu-captures"
        $before = @(Get-ChildItem $dir -Filter *.rdc -ErrorAction SilentlyContinue).Count
        $code = Run $log $game "--bot --mute --vsync 0 --exit-tick 120 --gpu-capture-at 30"
        $after = @(Get-ChildItem $dir -Filter *.rdc -ErrorAction SilentlyContinue)
        if ($after.Count -gt $before) { Remove-Item ($after | Sort-Object LastWriteTime | Select-Object -Last 1).FullName } # test captures are deleted
        return $code -eq 0 -and $after.Count -gt $before -and (Has $log "RenderDoc loaded")
    }
    Invoke-Check game "Game screen flow (title to match)" {
        param($log)
        $png = Join-Path $out "flow.png"
        $code = Run $log $game "--mute --frames 240 --ui-press 20:start,50:solo,90:start --capture `"$png`""
        return $code -eq 0 -and (Test-Path $png) -and (Has $log "Match 1 started") -and (Has $log "World ready") -and -not (Has $log "--ui-press: no widget")
    }
    # Live connection (contract between tools, mealkit/live/Server.h): hello + stats events using the game's build\live\ connection info; the connection info file disappears when done
    Invoke-Check game "Live connection (game)" {
        param($log)
        $p = Start-Process -FilePath $game -ArgumentList "--bot --mute --vsync 0 --size 640 360 --exit-tick 600" -PassThru -WindowStyle Hidden
        try {
            & powershell -NoProfile -ExecutionPolicy Bypass -File "$root\tools\live_probe.ps1" -Kind game -Wait 20 -Events 2 *> $log
            $code = $LASTEXITCODE
        } finally { $p.WaitForExit(60000) | Out-Null }
        $left = Test-Path (Join-Path $root "build\live\game-$($p.Id).json")
        return $code -eq 0 -and -not $left -and (Has $log "^event stats: ")
    }
    # Tool shortcuts (editor/Tools.h): finds manifests and recognizes tools with a different contract version
    Invoke-Check tools "Editor tool discovery (manifest)" {
        param($log)
        $dir = Join-Path $out "tools"
        Remove-Item -Recurse -Force $dir -ErrorAction SilentlyContinue
        New-Item -ItemType Directory -Force "$dir\good", "$dir\old" | Out-Null
        [IO.File]::WriteAllText("$dir\good\mealkit-tool.txt", "# MealKit tool manifest`n@format tool 1`nname Check Tool`nexe $game`nversion 1.0`nprotocol 1`nopens .table`n")
        [IO.File]::WriteAllText("$dir\old\mealkit-tool.txt", "@format tool 1`nname Old Tool`nexe old.exe`nprotocol 999`n")
        $code = Run $log $editor "--list-tools --tool-path `"$dir`""
        return $code -eq 0 -and (Has $log "^tool: Check Tool 1\.0 protocol 1 .*sandbox\.exe$") -and (Has $log "^tool: Old Tool .*\(protocol differs\)")
    }
    # Penpot plugin (ROADMAP "Game screen UI part 2"): converter tests (when Node.js is present), layout of screens exported from Penpot = Penpot boxes.
    # Compared with the English table (Penpot's source text is English, so text nodes draw the same text)
    Invoke-Check tools "Penpot plugin (converter tests)" {
        param($log)
        if (-not (Get-Command node -ErrorAction SilentlyContinue)) { return "SKIP: no Node.js" }
        $code = Run $log "node" "--test `"$root\tools\penpot-plugin\test\convert.test.ts`""
        return $code -eq 0
    }
    # Blender add-on (ROADMAP "Blender export add-on"): runs Blender LTS without a window for check/export tests (SKIP if absent).
    # Whether the supported range schema matches the engine is checked by the asset_schema unit test
    Invoke-Check tools "Blender add-on export" {
        param($log)
        $blender = $env:MEALKIT_BLENDER
        if (-not $blender) { $blender = "$env:ProgramFiles\Blender Foundation\Blender 5.2\blender.exe" }
        if (-not (Test-Path $blender)) { return "SKIP: no Blender 5.2 (set MEALKIT_BLENDER)" }
        $dir = Join-Path $out "blender-addon"
        if (Test-Path $dir) { Remove-Item $dir -Recurse -Force }
        # Run (cmd /s /c) can't wrap exe paths with spaces (Program Files), so it's called directly
        $ErrorActionPreference = "Continue" # lines Blender writes to stderr aren't treated as errors
        & $blender -b --factory-startup --python "$root\tools\blender-addon\test\test_export.py" -- $dir 2>&1 | Out-File $log -Encoding utf8
        return $LASTEXITCODE -eq 0 -and (Has $log "BLENDER ADDON ok")
    }
    Invoke-Check tools "Penpot screens = engine layout" {
        param($log)
        $code = Run $log $editor "--project `"$root`" --ui-check --lang en"
        return $code -eq 0 -and (Has $log "UI-CHECK .* -> ok") -and -not (Has $log "-> FAIL")
    }
    # Secondary: the board Penpot drew (art/ui/preview, half size) vs. the same screen drawn by the engine. Text rasterization/anti-aliasing differ, so it's loose
    # (pixels with channel difference over 48 at most 1%). Catches missing or badly misplaced nodes
    Invoke-Check tools "Penpot screens = engine pixels" {
        param($log)
        $previews = @(Get-ChildItem (Join-Path $root "art\ui\preview") -Filter *.png -ErrorAction SilentlyContinue)
        if ($previews.Count -eq 0) { return "SKIP: no art/ui/preview images" }
        foreach ($p in $previews) {
            $png = Join-Path $out ("engine-" + $p.Name)
            $code = Run "$log.render" $editor "--project `"$root`" --lang en --ui-render ui/$($p.BaseName).ui `"$png`""
            Get-Content "$log.render" | Add-Content $log
            if ($code -ne 0) { return $false }
            $diff = & powershell -NoProfile -ExecutionPolicy Bypass -File "$PSScriptRoot\compare_images.ps1" -A $p.FullName -B $png -Threshold 48 -MaxBadPercent 1 -MaxDiff 255 2>&1
            "$($p.Name): $diff" | Add-Content $log
            if ($LASTEXITCODE -ne 0) { return $false }
        }
        return $true
    }
    Invoke-Check editor "Editor capture (edit mode)" {
        param($log)
        $png = Join-Path $out "editor.png"
        $code = Run $log $editor "--project `"$root`" --frames 60 --select 25 --capture `"$png`""
        return $code -eq 0 -and (Test-Path $png) -and -not (Has $log "\[ERROR\]")
    }
    # Lookdev window (ROADMAP "Textures and materials part 3"): drawn to an offscreen target and shown in the window. No debug layer messages
    Invoke-Check render "Editor look dev (offscreen, DX12 debug layer)" {
        param($log)
        $png = Join-Path $out "editor-lookdev.png"
        $code = Run $log $editor "--project `"$root`" assets\scenes\styles.scene --frames 60 --select 3 --lookdev --debug --capture `"$png`""
        return $code -eq 0 -and (Test-Path $png) -and -not (Has $log "\[ERROR\]") -and -not (Has $log "\[D3D12\]")
    }
    # Graphics regression test (ROADMAP "Graphics regression tests"): draws fixed scenes (engine\tests\golden\shots.txt) without UI and compares with reference images.
    # On differences, build\check\golden\<name>-diff.png (red = differing pixels). Different GPUs/drivers may differ slightly, so there's a threshold
    $goldenDir = Join-Path $root "engine\tests\golden"
    function Compare-Golden([string]$log, [string]$exe, [string]$suffix) {
        $dir = Join-Path $out "golden$suffix"
        $code = Run $log $exe "--project `"$root`" --render-golden `"$goldenDir\shots.txt`" `"$dir`""
        if ($code -ne 0) { return "render failed" }
        $problems = @()
        foreach ($png in Get-ChildItem $dir -Filter *.png | Where-Object { $_.Name -notlike "*-diff.png" }) {
            $golden = Join-Path $goldenDir $png.Name
            if ($UpdateGolden -and $suffix -eq "") { Copy-Item $png.FullName $golden -Force; continue }
            if (-not (Test-Path $golden)) { $problems += "$($png.BaseName): no golden image (run check.ps1 -UpdateGolden)"; continue }
            $diff = Join-Path $dir ($png.BaseName + "-diff.png")
            $r = & powershell -NoProfile -ExecutionPolicy Bypass -File "$PSScriptRoot\compare_images.ps1" -A $golden -B $png.FullName -Threshold 8 -MaxBadPercent 0.5 -MaxDiff 255 -DiffOut $diff 2>&1
            Add-Content $log "$($png.Name): $($r | Select-Object -First 1)"
            if ($LASTEXITCODE -ne 0) { $problems += "$($png.BaseName): $($r | Select-Object -First 1)" }
        }
        if ($UpdateGolden -and $suffix -eq "") { return "SKIP: golden images updated in engine\tests\golden" }
        if ($problems.Count -gt 0) { return ($problems -join "; ") }
        return $true
    }
    Invoke-Check render "Graphics regression (golden images)" { param($log) Compare-Golden $log $editor "" }
    Invoke-Check editor "Editor play (solo, shadow)" {
        param($log)
        $png = Join-Path $out "editor-play.png"
        $code = Run $log $editor "--project `"$root`" --frames 180 --play --bot --capture `"$png`""
        return $code -eq 0 -and (Test-Path $png) -and (Has $log "Play mode") -and -not (Has $log "Shadow simulation diverged")
    }
    # RHI backends (ROADMAP stage 2 "RHI layer"): does the Vulkan editor draw the same scene the same as DX12, with no validation layer errors.
    # The editor edit screen is the same every run (the game screen changes over time). The status bar (fps) is excluded.
    $vkEditorBin = Join-Path $root "build\VulkanEditor\bin"
    $vkEditor = Join-Path $vkEditorBin "MealKitEditor.exe"
    Invoke-Check render "Vulkan editor build" { param($log) Build $log "VulkanEditor" }
    Invoke-Check render "Vulkan = DX12 (editor capture)" {
        param($log)
        # The window layout must match: uses the DX12 editor's layout file
        $ini = Join-Path $editorBin "mealkit_editor.ini"
        if (Test-Path $ini) { Copy-Item $ini $vkEditorBin -Force }
        $dx = Join-Path $out "backend-dx12.png"
        $vk = Join-Path $out "backend-vulkan.png"
        $common = "--project `"$root`" --frames 60 --select 25 --capture"
        if ((Run "$log.dx12" $editor "$common `"$dx`"") -ne 0 -or -not (Test-Path $dx)) { return "DX12 capture failed" }
        if ((Run "$log.vulkan" $vkEditor "$common `"$vk`"") -ne 0 -or -not (Test-Path $vk)) { return "Vulkan capture failed" }
        # The log lines (DLL path/port/token/time differ per run) and status bar are excluded. In the default 1600x900 layout the first log line starts at about y = 598
        $output = & powershell -NoProfile -ExecutionPolicy Bypass -File "$PSScriptRoot\compare_images.ps1" -A $dx -B $vk -IgnoreBottom 310 -DiffOut (Join-Path $out "backend-diff.png") 2>&1
        $output | Out-File -Encoding utf8 $log
        if ($LASTEXITCODE -ne 0) { return ($output | Select-Object -First 1) }
        return $true
    }
    Invoke-Check render "Graphics regression (Vulkan)" { param($log) Compare-Golden $log $vkEditor "-vulkan" }
    Invoke-Check render "Vulkan validation layer (play)" {
        param($log)
        $png = Join-Path $out "vulkan-play.png"
        $code = Run $log $vkEditor "--project `"$root`" --frames 180 --play --bot --debug --capture `"$png`""
        if (Has $log "validation layer unavailable") { return "SKIP: Vulkan SDK not installed (validation layer)" }
        if ($code -ne 0 -or -not (Has $log "Play mode")) { return "play failed" }
        $issues = @(Select-String -Path $log -Pattern "\[Vulkan\]")
        if ($issues.Count -gt 0) { return "$($issues.Count) validation message(s)" }
        return $true
    }
    Invoke-Check render "Vulkan validation layer (look dev)" {
        param($log)
        $png = Join-Path $out "vulkan-lookdev.png"
        $code = Run $log $vkEditor "--project `"$root`" assets\scenes\styles.scene --frames 60 --select 3 --lookdev --debug --capture `"$png`""
        if (Has $log "validation layer unavailable") { return "SKIP: Vulkan SDK not installed (validation layer)" }
        if ($code -ne 0 -or -not (Test-Path $png)) { return "capture failed" }
        $issues = @(Select-String -Path $log -Pattern "\[Vulkan\]")
        if ($issues.Count -gt 0) { return "$($issues.Count) validation message(s)" }
        return $true
    }
    Invoke-Check editor "Editor play (3 players)" {
        param($log)
        $png = Join-Path $out "editor-3p.png"
        $code = Run $log $editor "--project `"$root`" --frames 400 --play --players 3 --bot --capture `"$png`""
        return $code -eq 0 -and (Has $log "Play mode: 3 player") -and -not (Has $log "Play stopped")
    }
    # Build failure: putting forbidden code in sim/ makes SimCheck stop the build, and the editor must still be able to play with the previous DLL
    $bad = Join-Path $root "samples\sandbox\sim\ZzCheckFailure.cpp"
    try {
        Set-Content -Path $bad -Value "// Temporary file created by tools/check.ps1 (SimCheck failure test)`r`n#include <random>`r`n" -Encoding UTF8
        Invoke-Check editor "SimCheck stops a bad build" {
            param($log)
            $ok = -not (Build $log "Editor")
            return $ok -and (Has $log "SIM002")
        }
        Invoke-Check editor "Editor keeps the old DLL on failure" {
            param($log)
            Run $log $editor "--project `"$root`" --build-game --frames 30" | Out-Null
            return (Has $log "Build failed") -and (Has $log "Game module '.*' loaded")
        }
    } finally {
        Remove-Item $bad -Force -ErrorAction SilentlyContinue
    }
    Invoke-Check editor "Game code rebuild + load" {
        param($log)
        Run $log $editor "--project `"$root`" --build-game --frames 30" | Out-Null
        return (Has $log "Game code build finished") -and (Has $log "Game module '.*' loaded") -and -not (Has $log "Build failed")
    }
    Invoke-Check project "New project + upgrade" {
        param($log)
        $projects = Join-Path $env:TEMP "mealkit-check-projects" # must be outside the engine folder
        Remove-Item -Recurse -Force $projects -ErrorAction SilentlyContinue
        $output = & powershell -NoProfile -ExecutionPolicy Bypass -File "$PSScriptRoot\new_project.ps1" -Name CheckGame -Path $projects 2>&1
        $output += & powershell -NoProfile -ExecutionPolicy Bypass -File "$PSScriptRoot\upgrade_project.ps1" -Project "$projects\CheckGame" 2>&1
        $output | Out-File -Encoding utf8 $log
        $output += & powershell -NoProfile -ExecutionPolicy Bypass -File "$PSScriptRoot\check_layout.ps1" -Project "$projects\CheckGame" 2>&1
        $layoutOk = $LASTEXITCODE -eq 0 -and (($output -match "^OK layout: .* \(0 warning").Count -gt 0)
        # Game libraries: adding a header-only library works from view/ and is SimCheck SIM011 from sim/
        $lib = "$projects\CheckGame\third_party\checklib"
        New-Item -ItemType Directory -Force $lib | Out-Null
        Set-Content "$lib\checklib.h" "#pragma once`r`ninline int checkLib() { return 1; }" -Encoding UTF8
        Set-Content "$lib\LICENSE" "MIT" -Encoding UTF8
        Set-Content "$lib\mealkit.txt" "name checklib`r`nversion 1`r`nlicense MIT`r`nsource local`r`n" -Encoding UTF8
        Set-Content "$projects\CheckGame\source\sim\ZzLib.cpp" "#include `"checklib.h`"`r`n" -Encoding UTF8
        $sim = & "$root\build\Release\tools\mealkit_simcheck.exe" "$projects\stamp" --third-party $lib "$projects\CheckGame\source\sim\ZzLib.cpp" 2>&1
        $output += $sim
        $simOk = ($sim -match "SIM011").Count -gt 0
        # Source modules: a module's sim/ including game source/ is SIM013, a module not in requires is SIM014 (docs/MODULES.md)
        $mod = "$projects\CheckGame\modules\checkmod"
        New-Item -ItemType Directory -Force "$mod\sim" | Out-Null
        Set-Content "$mod\module.txt" "@format module 1`r`nname checkmod`r`nversion 0.1.0`r`nrequires`r`n" -Encoding UTF8
        Set-Content "$mod\sim\Bad.cpp" "#include `"sim/World.h`"`r`n#include `"othermod/sim/X.h`"`r`n" -Encoding UTF8
        New-Item -ItemType Directory -Force "$projects\CheckGame\modules\othermod\sim" | Out-Null
        Set-Content "$projects\CheckGame\modules\othermod\module.txt" "@format module 1`r`nname othermod`r`nversion 0.1.0`r`n" -Encoding UTF8
        $msim = & "$root\build\Release\tools\mealkit_simcheck.exe" "$projects\stamp" --modules "$projects\CheckGame\modules" "$mod\sim\Bad.cpp" 2>&1
        $output += $msim
        $simOk = $simOk -and ($msim -match "SIM013").Count -gt 0 -and ($msim -match "SIM014").Count -gt 0
        $output | Out-File -Encoding utf8 $log
        $ok = (Test-Path "$projects\CheckGame\.mealkit-template") -and (($output -match "^Done: 0 change").Count -gt 0) -and $layoutOk -and $simOk
        Remove-Item -Recurse -Force $projects -ErrorAction SilentlyContinue
        return $ok
    }
    Invoke-Check project "Empty template builds and plays" {
        param($log)
        # tools\new_project.ps1 -Template empty: its own sim/view/app + the screen flow shared with samples/sandbox. Builds the game DLL against
        # the engine's Editor build, plays 2 bot players in the editor (shadow simulation on), then checks upgrade and layout
        if (-not (Test-Path "$root\build\Editor\MealKitTargets.cmake")) { return "SKIP: no Editor build (build.bat Editor)" }
        $projects = Join-Path $env:TEMP "mealkit-check-empty" # must be outside the engine folder
        Remove-Item -Recurse -Force $projects -ErrorAction SilentlyContinue
        $output = & powershell -NoProfile -ExecutionPolicy Bypass -File "$PSScriptRoot\new_project.ps1" -Name EmptyCheck -Path $projects -Template empty 2>&1
        $output += & powershell -NoProfile -ExecutionPolicy Bypass -File "$PSScriptRoot\upgrade_project.ps1" -Project "$projects\EmptyCheck" -Source 2>&1
        $output += & powershell -NoProfile -ExecutionPolicy Bypass -File "$PSScriptRoot\check_layout.ps1" -Project "$projects\EmptyCheck" 2>&1
        $output | Out-File -Encoding utf8 $log
        $created = ($output -match "empty template").Count -gt 0 -and ($output -match "^Done: 0 change").Count -gt 0 -and
                   ($output -match "^OK layout: .* \(0 warning").Count -gt 0
        $build = "$log.build.txt"
        $code = Run $build "$projects\EmptyCheck\build.bat" "Editor"
        $built = $code -eq 0 -and (Has $build "Build OK")
        $play = "$log.play.txt"
        if ($built) { Run $play $editor "--project `"$projects\EmptyCheck`" --play --players 2 --bot --frames 300" | Out-Null }
        $played = $built -and (Has $play "Game module 'EmptyCheck' loaded") -and (Has $play "Match 1 started with 2") -and -not (Has $play "ERROR|desync")
        Get-Content $build, $play -ErrorAction SilentlyContinue | Add-Content $log
        if ($created -and $played) { Remove-Item -Recurse -Force $projects -ErrorAction SilentlyContinue }
        return $created -and $played
    }
    Invoke-Check project "Module tool (add / update / remove)" {
        param($log)
        # tools\module.ps1 (docs/MODULES.md): add from a folder → the game edits one file → update to a new version (edited files become .new, vanished files are deleted)
        # → removal refused while source/ includes it → re-add from a git tag
        $ErrorActionPreference = "Continue" # refusals and git notices go to stderr. Results are checked via exit codes and files
        $t = Join-Path $env:TEMP "mealkit-check-modules"
        Remove-Item -Recurse -Force $t -ErrorAction SilentlyContinue
        $proj = "$t\Game"; $src = "$t\mod-cool"
        New-Item -ItemType Directory -Force "$proj\source\sim", "$src\sim", "$src\tests" | Out-Null
        [IO.File]::WriteAllText("$proj\mealkit.project", "@format project 1`nname Game`n")
        [IO.File]::WriteAllText("$src\module.txt", "@format module 1`nname cool`nversion 0.1.0`n")
        foreach ($f in "A", "B", "Gone") { [IO.File]::WriteAllText("$src\sim\$f.h", "$f 1") }
        [IO.File]::WriteAllText("$src\tests\T.cpp", "t")
        $tool = "$PSScriptRoot\module.ps1"
        $modOut = [Collections.Generic.List[string]]::new() # $out is check.ps1's log folder
        function Mod { & powershell -NoProfile -ExecutionPolicy Bypass -File $tool @args -Project $proj 2>&1 | ForEach-Object { $modOut.Add("$_") }; return $LASTEXITCODE }
        $ok = (Mod add $src) -eq 0 -and -not (Test-Path "$proj\modules\cool\tests") -and (Test-Path "$proj\modules\cool\module.files")
        [IO.File]::WriteAllText("$proj\modules\cool\sim\B.h", "B mine")
        [IO.File]::WriteAllText("$src\module.txt", "@format module 1`nname cool`nversion 0.2.0`n")
        foreach ($f in "A", "B") { [IO.File]::WriteAllText("$src\sim\$f.h", "$f 2") }
        Remove-Item "$src\sim\Gone.h"
        $ok = $ok -and (Mod update cool) -eq 0
        $ok = $ok -and (Get-Content "$proj\modules\cool\sim\A.h") -eq "A 2" -and (Get-Content "$proj\modules\cool\sim\B.h") -eq "B mine" -and
            (Test-Path "$proj\modules\cool\sim\B.h.new") -and -not (Test-Path "$proj\modules\cool\sim\Gone.h")
        [IO.File]::WriteAllText("$proj\source\sim\W.h", "#include `"cool/sim/A.h`"`n")
        $ok = $ok -and (Mod remove cool) -ne 0 -and (Test-Path "$proj\modules\cool")
        $ok = $ok -and (Mod remove cool -Force) -eq 0 -and -not (Test-Path "$proj\modules\cool")
        Push-Location $src
        git init -q; git -c user.name=check -c user.email=check@localhost add -A 2>$null; git -c user.name=check -c user.email=check@localhost commit -qm v 2>$null; git tag v0.2.0
        Pop-Location
        $ok = $ok -and (Mod add "file:///$($src.Replace('\', '/'))#v0.2.0") -eq 0 -and
            ((Get-Content "$proj\modules\cool\module.files") -match "^source git .* v0\.2\.0 [0-9a-f]{40}$").Count -eq 1
        # Registry names: find/add via a local index.json (shaped like MealKit-Modules tools/build_index.py), refused if the commit differs
        $commit = (git -C $src rev-parse HEAD).Trim()
        $repo = "file:///$($src.Replace('\', '/'))"
        $index = "$t\index.json"
        $writeIndex = { param($c) [IO.File]::WriteAllText($index, (@{ format = 1; modules = @(@{ name = "cool"; repo = $repo; author = "check"; title = "Cool";
            summary = "check module"; tags = @("timer"); latest = "0.2.0"; versions = @(@{ version = "0.2.0"; tag = "v0.2.0"; commit = $c;
            engine = ((Get-Content "$root\VERSION") -replace '^(\d+\.\d+).*$', '$1'); requires = @() }) }) } | ConvertTo-Json -Depth 6)) }
        & $writeIndex $commit
        $ok = $ok -and (Mod remove cool -Force) -eq 0 -and (Mod search timer -Index $index) -eq 0 -and ($modOut -match "^cool 0\.2\.0").Count -ge 1
        $ok = $ok -and (Mod add cool -Index $index) -eq 0 -and
            ((Get-Content "$proj\modules\cool\module.files") -match "^source registry cool 0\.2\.0 $commit ").Count -eq 1
        & $writeIndex ("0" * 40)
        $ok = $ok -and (Mod remove cool -Force) -eq 0 -and (Mod add cool -Index $index) -ne 0 -and -not (Test-Path "$proj\modules\cool")
        $modOut | Out-File -Encoding utf8 $log
        Remove-Item -Recurse -Force $t -ErrorAction SilentlyContinue
        return $ok
    }
    Invoke-Check export "Export build" {
        param($log)
        $exe = Join-Path $root "export\sandbox\sandbox.exe"
        if (-not ((Build $log "Export") -and (Test-Path $exe))) { return $false }
        # Shipping builds have no live connection server (MEALKIT_LIVE=0)
        $text = [Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($exe))
        if ($text.Contains("Live server (")) { return "export\sandbox\sandbox.exe contains the live server (MEALKIT_LIVE should be 0)" }
        return $true
    }
}

$failed = @($results | Where-Object { -not $_.Ok })
# If all determinism groups passed, record the key (the next check skips if the key is the same)
$detResults = @($results | Where-Object { $_.Group -eq "det" })
if ($detKey -and $detResults.Count -gt 0 -and @($detResults | Where-Object { -not $_.Ok }).Count -eq 0) {
    New-Item -ItemType Directory -Force $stateDir | Out-Null
    Set-Content -Path $detKeyFile -Value $detKey -Encoding ASCII
}
$skipped = @($results | Where-Object { $_.Skipped })
$warned = @($results | Where-Object { $_.Warned })
$total = ($results | Measure-Object -Property Seconds -Sum).Sum
Write-Host ("{0} passed, {1} warned, {2} skipped, {3} failed, {4:N0}s total" -f ($results.Count - $failed.Count - $skipped.Count - $warned.Count), $warned.Count, $skipped.Count, $failed.Count, $total) `
    -ForegroundColor $(if ($failed.Count -eq 0) { "Green" } else { "Red" })
if ($failed.Count -gt 0) { exit 1 }
exit 0

# Engine module check (docs/ENGINE_MODULES.md "Manifest" and "Checks"). Called by check.ps1 and CI. Replaces the old folder order check.
#   powershell -ExecutionPolicy Bypass -File tools\check_modules.ps1     exit code 1 on problems
# Every folder in engine/core/ (kernel) and modules/ is an engine module with an engine-module.txt. This checks that:
#   - each manifest parses (format line, known keys, name = folder, kind, semver, kernel = engine MINOR)
#   - requires names exist and form a graph without cycles; kernel modules (core, platform, jobs) require nothing and are implied for all
#   - each file includes only its own module, the kernel and its requires (plus transitional ones)
#   - kinds: kernel includes only kernel; sim only kernel and sim; view anything but editor; tool anything
#   - transitional lists only includes that break the kind rule and are still there (stale entries fail)
#   - core files (MEALKIT_CORE_SOURCES in engine/CMakeLists.txt, linked by headless/Linux/macOS) include only core headers
# platform/<os>/ implementation files implement headers from several modules (platform/Socket.h etc.), so they're excluded; check_platform.ps1 covers them.
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
. "$PSScriptRoot\engine_modules_common.ps1"
$problems = @()
$warnings = @()

$version = ([IO.File]::ReadAllText((Join-Path $root "VERSION"))).Trim()
$engineMinor = ($version -split '\.')[0..1] -join '.'
$known = @("name", "version", "kernel", "kind", "layer", "requires", "transitional", "optional", "tag", "summary", "provides")
$kinds = @("kernel", "sim", "view", "tool")
# Layers, low to high (docs/ENGINE_MODULES.md "Manifest"): a module includes only its own layer and the ones below. The kernel modules
# (platform, core, jobs) form the base and include each other
$layers = @("platform", "kernel", "core", "framework", "app")

# ---- Read manifests
$mods = [ordered]@{}
foreach ($dir in Get-EngineModules $root | ForEach-Object { Get-Item $_.Dir }) {
    $path = Join-Path $dir.FullName "engine-module.txt"
    $rel = $path.Substring($root.Length + 1)
    if (-not (Test-Path $path)) { $problems += "${rel}: missing (every engine folder is a module)"; continue }
    $m = @{ name = ""; kind = ""; layer = ""; version = ""; kernel = ""; requires = @(); optional = @(); transitional = @(); rel = $rel }
    $sawFormat = $false
    $n = 0
    foreach ($line in [IO.File]::ReadAllLines($path)) {
        $n++
        $t = $line.Trim()
        if ($t -eq "" -or $t.StartsWith("#")) { continue }
        if (-not $sawFormat) {
            if ($t -ne "@format engine-module 1") { $problems += "${rel}:${n}: first line must be '@format engine-module 1'" }
            $sawFormat = $true
            continue
        }
        $parts = $t -split '\s+'
        $key = $parts[0]
        $rest = @($parts | Select-Object -Skip 1)
        if ($known -notcontains $key) { $problems += "${rel}:${n}: unknown key '$key'"; continue }
        switch ($key) {
            "requires" {
                if ($rest.Count % 2 -ne 0) { $problems += "${rel}:${n}: requires is 'name minVersion' pairs"; break }
                for ($i = 0; $i -lt $rest.Count; $i += 2) {
                    if ($rest[$i + 1] -notmatch '^\d+\.\d+(\.\d+)?$') { $problems += "${rel}:${n}: '$($rest[$i + 1])' isn't a version for $($rest[$i])" }
                    $m.requires += $rest[$i]
                }
            }
            "optional" { # modules a project may leave out: includes from them sit inside #if MEALKIT_HAS_<NAME> (docs/ENGINE_MODULES.md "Presets")
                for ($i = 0; $i -lt $rest.Count; $i += 2) { $m.optional += $rest[$i] }
            }
            "transitional" { $m.transitional = $rest }
            "summary" { }
            "provides" { }
            default { $m[$key] = ($rest -join " ") }
        }
    }
    if (-not $sawFormat) { $problems += "${rel}: empty" }
    if ($m.name -ne $dir.Name) { $problems += "${rel}: name '$($m.name)' must match the folder '$($dir.Name)'" }
    if ($kinds -notcontains $m.kind) { $problems += "${rel}: kind '$($m.kind)' must be one of $($kinds -join ', ')" }
    if ($layers -notcontains $m.layer) { $problems += "${rel}: layer '$($m.layer)' must be one of $($layers -join ', ')" }
    if ($m.version -notmatch '^\d+\.\d+\.\d+$') { $problems += "${rel}: version '$($m.version)' isn't semver" }
    if ($m.kernel -ne $engineMinor) { $problems += "${rel}: kernel '$($m.kernel)' doesn't match the engine $engineMinor (VERSION)" }
    $mods[$dir.Name] = $m
}

$kernel = @($mods.Keys | Where-Object { $mods[$_].kind -eq "kernel" })
foreach ($name in $mods.Keys) {
    $m = $mods[$name]
    if ($m.kind -eq "kernel" -and $m.requires.Count) { $problems += "$($m.rel): kernel modules require nothing" }
    foreach ($r in $m.requires) {
        if (-not $mods.Contains($r)) { $problems += "$($m.rel): requires unknown module '$r'" }
        elseif ($mods[$r].kind -eq "kernel") { $problems += "$($m.rel): '$r' is kernel, which every module has; don't list it" }
        elseif ($r -eq $name) { $problems += "$($m.rel): requires itself" }
    }
    foreach ($r in $m.optional) { if (-not $mods.Contains($r)) { $problems += "$($m.rel): optional names unknown module '$r'" } }
    foreach ($r in $m.transitional) { if (-not $mods.Contains($r)) { $problems += "$($m.rel): transitional names unknown module '$r'" } }
}

# ---- Cycles (depth-first over requires + transitional) and a dependency order for the report
$state = @{}
$order = New-Object System.Collections.Generic.List[string]
function Visit([string]$name, [string[]]$path) {
    if ($state[$name] -eq 2) { return }
    if ($state[$name] -eq 1) { $script:problems += "cycle: " + (($path + $name) -join " -> "); return }
    $state[$name] = 1
    $m = $mods[$name]
    foreach ($r in @($m.requires) + @($m.optional) + @($m.transitional)) { if ($mods.Contains($r)) { Visit $r ($path + $name) } }
    $state[$name] = 2
    $order.Add($name)
}
foreach ($name in $mods.Keys) { Visit $name @() }

# ---- Includes against manifests and kinds
function KindAllows([string]$from, [string]$to) {
    switch ($from) {
        "kernel" { return $to -eq "kernel" }
        "sim" { return $to -eq "kernel" -or $to -eq "sim" }
        "view" { return $true }
        default { return $true }
    }
}
$files = Get-EngineModuleFiles $root |
    Where-Object { $_.FullName -notmatch '\\mealkit\\platform\\(win32|posix)\\' }
$edges = 0
$used = @{}
foreach ($f in $files) {
    $rel = $f.FullName.Substring($root.Length + 1)
    $from = $f.Module
    if (-not $mods.Contains($from)) { continue } # reported above
    $m = $mods[$from]
    $n = 0
    foreach ($line in [IO.File]::ReadAllLines($f.FullName)) {
        $n++
        $code = ($line -replace '//.*$', '')
        if ($code -notmatch '^\s*#\s*include\s*["<]mealkit/(\w+)/') { continue }
        $to = $Matches[1]
        if ($to -eq $from) { continue }
        $edges++
        if (-not $mods.Contains($to)) { $problems += "${rel}:${n}: includes mealkit/$to/, which isn't an engine module"; continue }
        $used["$from>$to"] = $true
        $toKind = $mods[$to].kind
        if ($toKind -eq "kernel") { continue }
        $declared = ($m.requires -contains $to) -or ($m.optional -contains $to)
        $transitional = $m.transitional -contains $to
        if (-not $declared -and -not $transitional) { $problems += "${rel}:${n}: $from includes $to/, not in its requires ($($m.rel))"; continue }
        if ($to -eq "editor") { $problems += "${rel}:${n}: $from includes editor/ (only the editor app may)"; continue }
        $fromRank = [array]::IndexOf($layers, $m.layer); $toRank = [array]::IndexOf($layers, $mods[$to].layer)
        if ($toRank -gt $fromRank -and -not $transitional) {
            $problems += "${rel}:${n}: $from ($($m.layer)) includes $to ($($mods[$to].layer)), a higher layer"
        }
        if (-not (KindAllows $m.kind $toKind) -and -not $transitional) {
            $problems += "${rel}:${n}: $from ($($m.kind)) includes $to ($toKind); $($m.kind) modules can't depend on $toKind modules"
        }
    }
}
foreach ($name in $mods.Keys) {
    $m = $mods[$name]
    foreach ($r in $m.requires) { if (-not $used.ContainsKey("$name>$r")) { $warnings += "$($m.rel): requires $r but includes nothing from it" } }
    foreach ($r in $m.transitional) {
        if (-not $used.ContainsKey("$name>$r")) { $problems += "$($m.rel): transitional $r is no longer included; remove it" }
        elseif ($mods.Contains($r) -and (KindAllows $m.kind $mods[$r].kind)) { $problems += "$($m.rel): transitional $r is allowed by kind; move it to requires" }
    }
}

# ---- Core files (MEALKIT_CORE_SOURCES) include only core headers. Module rules can't see within one folder (game/Headless.cpp -> game/Frontend.h).
$cmake = [IO.File]::ReadAllText((Join-Path $root "engine\CMakeLists.txt")) -replace '#[^\r\n]*', '' # so parentheses in comments don't break the list
$core = @{}
foreach ($block in [regex]::Matches($cmake, '(?s)(?:set|list\(APPEND)\s*\(?\s*MEALKIT_CORE_SOURCES\b(.*?)\)')) {
    foreach ($mm in [regex]::Matches($block.Groups[1].Value, '(?:include|src)/(mealkit/[\w/]+\.(?:h|cpp))')) { $core[$mm.Groups[1].Value] = $true }
}
$coreEdges = 0
foreach ($f in $files) {
    if ($f.FullName -notmatch '\\(?:include|src)\\(mealkit\\.+)$') { continue }
    $key = $Matches[1].Replace('\', '/')
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

$warnings | ForEach-Object { Write-Host "warning: $_" -ForegroundColor Yellow }
if ($problems.Count -gt 0) {
    $problems | ForEach-Object { Write-Host $_ }
    Write-Host "FAIL: $($problems.Count) problem(s) with engine modules" -ForegroundColor Red
    exit 1
}
Write-Host "OK: $($mods.Count) modules ($($kernel -join '+') kernel), $($files.Count) files, $edges cross-module includes match the manifests; $($core.Count) core files include only core headers ($coreEdges includes)" -ForegroundColor Green
Write-Host "dependency order: $($order -join ' < ')"
exit 0

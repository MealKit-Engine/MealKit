# CI tests (one build): unit tests, fixed input recording replay, headless mp_test. Runs on Windows PowerShell 5.1 and pwsh 7 (Linux/macOS).
#   pwsh tools/ci_test.ps1 -Bin build/Headless/bin -Name linux-clang [-Out ci-out/linux-clang] [-NoHeadless]
# Results (-Out folder, uploaded as artifacts by CI):
#   detmath-bits.txt          the "detmath-bits <hash>" line of mealkit_tests (must be the same on all builds)
#   <sample>-<recording>.hashes.txt   per-tick hashes from replaying samples/*/replays/*.rec (must be the same on all OSes/CPUs/compilers)
#   *.log                     output of each run
# Comparison between builds is tools/ci_compare.ps1. Exit code 1 if anything fails.
param(
    [Parameter(Mandatory = $true)][string]$Bin,
    [Parameter(Mandatory = $true)][string]$Name,
    [string]$Out = "",
    [switch]$NoHeadless # Editor build: no headless target (unit tests only)
)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$exeSuffix = if ($env:OS -eq "Windows_NT") { ".exe" } else { "" }
$Bin = (Resolve-Path $Bin).Path
if ($Out -eq "") { $Out = Join-Path $root "build/ci/$Name" }
New-Item -ItemType Directory -Force $Out | Out-Null
$Out = (Resolve-Path $Out).Path
$failed = @()
$ErrorActionPreference = "Continue" # from here, failures are collected and reported at the end

function Run-Exe([string]$exe, [string[]]$arguments, [string]$log) {
    $output = & $exe @arguments 2>&1
    $code = $LASTEXITCODE
    $output | ForEach-Object { "$_" } | Out-File -Encoding utf8 $log
    return $code
}

# --- Unit tests + detmath-bits
$tests = Join-Path $Bin "mealkit_tests$exeSuffix"
$log = Join-Path $Out "unit-tests.log"
$code = Run-Exe $tests @() $log
$bits = Select-String -Path $log -Pattern "^detmath-bits (\w+)" | Select-Object -First 1
if ($code -ne 0 -or -not $bits) {
    $failed += "unit tests (exit $code)"
} else {
    $bits.Matches[0].Groups[1].Value | Out-File -Encoding ascii -NoNewline (Join-Path $Out "detmath-bits.txt")
    Write-Host "unit tests: PASS, detmath-bits $($bits.Matches[0].Groups[1].Value)"
}

if (-not $NoHeadless) {
    # --- Fixed input recording replay: compare with the per-tick hashes in the recording (REPLAY ok), and keep a hash log to compare with other builds
    $replays = Get-ChildItem -Path (Join-Path $root "samples") -Recurse -Filter *.rec |
        Where-Object { $_.Directory.Name -eq "replays" } | Sort-Object FullName
    if ($replays.Count -eq 0) { $failed += "no samples/*/replays/*.rec" }
    foreach ($rec in $replays) {
        $sample = $rec.Directory.Parent.Name
        $headless = Join-Path $Bin "${sample}_headless$exeSuffix"
        $id = "$sample-$($rec.BaseName)"
        $hashes = Join-Path $Out "$id.hashes.txt"
        $log = Join-Path $Out "$id.replay.log"
        $code = Run-Exe $headless @("--replay-inputs", $rec.FullName, "--hash-log", $hashes) $log
        $line = Select-String -Path $log -Pattern "^REPLAY " | Select-Object -Last 1
        $text = if ($line) { $line.Line } else { "(no REPLAY line)" }
        Write-Host "replay ${id}: $text"
        if ($code -ne 0 -or -not ($text -match "^REPLAY ok")) { $failed += "replay $id ($text)" }
    }

    # --- Scenario tests (samples/*/tests/*.scenario): state checks on the headless World
    $scenarios = Get-ChildItem -Path (Join-Path $root "samples") -Recurse -Filter *.scenario |
        Where-Object { $_.Directory.Name -eq "tests" } | Sort-Object FullName
    foreach ($file in $scenarios) {
        $sample = $file.Directory.Parent.Name
        $headless = Join-Path $Bin "${sample}_headless$exeSuffix"
        $id = "$sample-$($file.BaseName)"
        $log = Join-Path $Out "$id.scenario.log"
        $code = Run-Exe $headless @("--test", $file.FullName) $log
        $line = Select-String -Path $log -Pattern "^SCENARIO (ok|FAIL) .* (checks|failed)=" | Select-Object -Last 1
        $text = if ($line) { $line.Line } else { "(no SCENARIO line)" }
        Write-Host "scenario ${id}: $text"
        if ($code -ne 0 -or -not ($text -match "^SCENARIO ok")) { $failed += "scenario $id ($text)" }
    }

    # --- Headless mp_test: host 1 + clients 3 within this runner, with different physics thread counts
    $mpExe = Join-Path $Bin "sandbox_headless$exeSuffix"
    $log = Join-Path $Out "mp_test.log"
    # mp_test writes with Write-Host, so all streams (*>&1) are captured
    $output = & (Join-Path $PSScriptRoot "mp_test.ps1") -Clients 3 -Ticks 900 -Headless -Exe $mpExe *>&1 |
        ForEach-Object { "$_" }
    $code = $LASTEXITCODE
    $output | Out-File -Encoding utf8 $log
    $output | ForEach-Object { Write-Host "  $_" }
    if ($code -ne 0 -or @($output -match "^PASS").Count -eq 0) {
        $failed += "mp_test -Headless"
        # On failure, keep each peer's output (mp_test log folder: <build>/mp_test)
        $mpLogs = Join-Path (Split-Path -Parent (Split-Path -Parent $Bin)) "mp_test"
        if (Test-Path $mpLogs) {
            Copy-Item (Join-Path $mpLogs "*") $Out -ErrorAction SilentlyContinue
            foreach ($f in Get-ChildItem $mpLogs -File | Sort-Object Name) {
                Write-Host "----- $($f.Name) (last 15 lines)"
                Get-Content $f.FullName -Tail 15 | ForEach-Object { Write-Host "  $_" }
            }
        }
    }
}

if ($failed.Count -gt 0) {
    Write-Host "CI test $Name FAILED: $($failed -join '; ')" -ForegroundColor Red
    exit 1
}
Write-Host "CI test $Name PASS" -ForegroundColor Green
exit 0

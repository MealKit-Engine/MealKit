# CI 테스트 (한 빌드): 단위 테스트, 고정 입력 기록 재생, 헤드리스 mp_test. Windows PowerShell 5.1과 pwsh 7(Linux·macOS)에서 돈다.
#   pwsh tools/ci_test.ps1 -Bin build/Headless/bin -Name linux-clang [-Out ci-out/linux-clang] [-NoHeadless]
# 결과(-Out 폴더, CI가 아티팩트로 올린다):
#   detmath-bits.txt          mealkit_tests의 "detmath-bits <hash>" 줄 (모든 빌드에서 같아야 한다)
#   <샘플>-<기록>.hashes.txt   samples/*/replays/*.rec 재생의 틱별 해시 (모든 OS·CPU·컴파일러에서 같아야 한다)
#   *.log                     각 실행의 출력
# 빌드끼리의 비교는 tools/ci_compare.ps1. 하나라도 실패하면 종료 코드 1.
param(
    [Parameter(Mandatory = $true)][string]$Bin,
    [Parameter(Mandatory = $true)][string]$Name,
    [string]$Out = "",
    [switch]$NoHeadless # Editor 빌드: 헤드리스 타깃이 없다 (단위 테스트만)
)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$exeSuffix = if ($env:OS -eq "Windows_NT") { ".exe" } else { "" }
$Bin = (Resolve-Path $Bin).Path
if ($Out -eq "") { $Out = Join-Path $root "build/ci/$Name" }
New-Item -ItemType Directory -Force $Out | Out-Null
$Out = (Resolve-Path $Out).Path
$failed = @()
$ErrorActionPreference = "Continue" # 여기부터는 실패를 모아서 끝에 보고한다

function Run-Exe([string]$exe, [string[]]$arguments, [string]$log) {
    $output = & $exe @arguments 2>&1
    $code = $LASTEXITCODE
    $output | ForEach-Object { "$_" } | Out-File -Encoding utf8 $log
    return $code
}

# --- 단위 테스트 + detmath-bits
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
    # --- 고정 입력 기록 재생: 기록에 든 틱별 해시와 비교하고(REPLAY ok), 해시 로그를 남겨 다른 빌드와 비교한다
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

    # --- 헤드리스 mp_test: 이 러너 안에서 호스트 1 + 클라이언트 3, 물리 스레드 수를 다르게
    $mpExe = Join-Path $Bin "sandbox_headless$exeSuffix"
    $log = Join-Path $Out "mp_test.log"
    # mp_test는 Write-Host로 쓰므로 모든 스트림(*>&1)을 받는다
    $output = & (Join-Path $PSScriptRoot "mp_test.ps1") -Clients 3 -Ticks 900 -Headless -Exe $mpExe *>&1 |
        ForEach-Object { "$_" }
    $code = $LASTEXITCODE
    $output | Out-File -Encoding utf8 $log
    $output | ForEach-Object { Write-Host "  $_" }
    if ($code -ne 0 -or @($output -match "^PASS").Count -eq 0) {
        $failed += "mp_test -Headless"
        # 실패하면 각 피어의 출력을 남긴다 (mp_test 로그 폴더: <build>/mp_test)
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

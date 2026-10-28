# CI용 고정 입력 기록 만들기: 헤드리스 호스트 1 + 클라이언트 N-1을 봇 입력으로 돌리고, 호스트가 확정 입력과 틱별 해시를 기록한다.
# 시뮬레이션 결과를 일부러 바꿨을 때 저장소의 기록(samples/*/replays/*.rec)을 다시 만든다 (문서 저장소 "엔진 개발 > 테스트").
#   powershell -ExecutionPolicy Bypass -File tools\record_replay.ps1                (sandbox arena_4p_seed7.rec 다시 만들기)
#   ... -Out <파일.rec> [-Players 4] [-Seed 7] [-Ticks 900] [-Exe <게임>_headless.exe]
param(
    [string]$Out = "",
    [int]$Players = 4,
    [int]$Seed = 7,
    [int]$Ticks = 900,
    [int]$Port = 27140,
    [string]$Exe = ""
)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
if ($Exe -eq "") { $Exe = Join-Path $root "build\Release\bin\sandbox_headless.exe" }
if ($Out -eq "") { $Out = Join-Path $root "samples\sandbox\replays\arena_4p_seed7.rec" }
if (-not (Test-Path $Exe)) { Write-Error "Build first: $Exe not found"; exit 1 }
$Exe = (Resolve-Path $Exe).Path
New-Item -ItemType Directory -Force (Split-Path -Parent $Out) | Out-Null
$Out = [IO.Path]::GetFullPath($Out)
$logDir = Join-Path $root "build\record_replay"
New-Item -ItemType Directory -Force $logDir | Out-Null
Get-ChildItem $logDir -File | Remove-Item -Force

$procs = @()
$procs += Start-Process -FilePath $Exe -PassThru -NoNewWindow `
    -ArgumentList "--host $Port --local --players $Players --auto-start $Players --seed $Seed --exit-tick $Ticks --record-inputs `"$Out`"" `
    -RedirectStandardOutput "$logDir\host.txt" -RedirectStandardError "$logDir\host_err.txt"
Start-Sleep -Milliseconds 500
for ($i = 1; $i -lt $Players; $i++) {
    $procs += Start-Process -FilePath $Exe -PassThru -NoNewWindow `
        -ArgumentList "--join 127.0.0.1:$Port --threads $i --exit-tick $Ticks" `
        -RedirectStandardOutput "$logDir\client$i.txt" -RedirectStandardError "$logDir\client${i}_err.txt"
}
foreach ($p in $procs) {
    if (-not $p.WaitForExit(120000)) { $p.Kill(); Write-Warning "Process $($p.Id) timed out" }
}
$finals = @(Get-ChildItem $logDir -Filter *.txt | Where-Object { $_.Name -notmatch "_err" } |
    ForEach-Object { (Select-String -Path $_.FullName -Pattern "^FINAL" | Select-Object -Last 1).Line } | Sort-Object -Unique)
if ($finals.Count -ne 1 -or $finals[0] -notmatch "desync=0") { Write-Host "FAIL: peers differ, see $logDir" -ForegroundColor Red; exit 1 }

# 기록을 바로 재생해 본다
$ErrorActionPreference = "Continue" # 경고 로그(stderr)를 PowerShell 5.1이 오류로 끊지 않게. 결과는 REPLAY 줄로 본다
$replay = & $Exe --replay-inputs $Out 2>&1 | ForEach-Object { "$_" }
if (-not ($replay -match "^REPLAY ok")) { Write-Host "FAIL: replay of $Out" -ForegroundColor Red; exit 1 }
Write-Host "Recorded $Out ($Players players, seed $Seed, $Ticks ticks): $($finals[0])" -ForegroundColor Green
exit 0

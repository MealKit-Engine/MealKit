# Makes fixed input recordings for CI: runs headless host 1 + N-1 clients with bot input, and the host records confirmed inputs and per-tick hashes.
# Regenerates the repo's recordings (samples/*/replays/*.rec) when simulation results were changed on purpose (docs repo "Engine development > Testing").
#   powershell -ExecutionPolicy Bypass -File tools\record_replay.ps1                (regenerates sandbox arena_4p_seed7.rec)
#   ... -Out <file.rec> [-Players 4] [-Seed 7] [-Ticks 900] [-Exe <game>_headless.exe]
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

# Replays the recording right away to check
$ErrorActionPreference = "Continue" # so PowerShell 5.1 doesn't abort on warning logs (stderr) as errors. Results come from the REPLAY line
$replay = & $Exe --replay-inputs $Out 2>&1 | ForEach-Object { "$_" }
if (-not ($replay -match "^REPLAY ok")) { Write-Host "FAIL: replay of $Out" -ForegroundColor Red; exit 1 }
Write-Host "Recorded $Out ($Players players, seed $Seed, $Ticks ticks): $($finals[0])" -ForegroundColor Green
exit 0

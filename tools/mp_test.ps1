# 멀티플레이 결정론 테스트: 같은 PC에서 호스트 1 + 클라이언트 N 을 봇 입력으로 돌리고
# 모두 같은 틱에서 같은 상태 해시를 내는지 확인한다.
# 클라이언트 i는 물리 스레드 i개를 쓴다 (호스트는 자동) → 스레드 수가 달라도 결과가 같은지도 확인된다.
#   powershell -ExecutionPolicy Bypass -File tools\mp_test.ps1 [-Clients 1] [-Ticks 900] [-Config Release] [-Scene <file>] [-Tuning <file>]
# 게임 프로젝트의 실행 파일을 시험하려면 -Exe <프로젝트>\build\Release\bin\<이름>.exe
# 다른 컴파일러 빌드와 맞붙이려면 -ClientExe build\ClangCL\bin\sandbox.exe (호스트는 -Exe, 클라이언트는 이것)
# 창 없이(헤드리스) 돌리려면 -Headless (각 실행 파일에 --headless를 붙인다. 입력이 허락하는 만큼 빨리 돈다)
param(
    [int]$Clients = 1,
    [int]$Ticks = 900,
    [int]$Bodies = 0, # 0 = 씬이 정한 상자 수
    [int]$Port = 27115,
    [string]$Config = "Release",
    [string]$Exe = "",
    [string]$ClientExe = "",
    [string]$Scene = "",
    [string]$Tuning = "",
    [switch]$Headless
)

if ($Exe -eq "") {
    $root = Split-Path -Parent $PSScriptRoot
    $Exe = Join-Path $root "build\$Config\bin\sandbox.exe"
}
if (-not (Test-Path $Exe)) { Write-Error "Build first: $Exe not found"; exit 1 }
$exe = (Resolve-Path $Exe).Path
$clientPath = $exe
if ($ClientExe -ne "") {
    if (-not (Test-Path $ClientExe)) { Write-Error "Build first: $ClientExe not found"; exit 1 }
    $clientPath = (Resolve-Path $ClientExe).Path
    Write-Host "host:    $exe"
    Write-Host "clients: $clientPath"
}
# 로그: <build 폴더>\mp_test (실행 파일은 <build>\<Config>\bin 에 있다)
$logDir = Join-Path (Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $exe))) "mp_test"
New-Item -ItemType Directory -Force $logDir | Out-Null

$players = $Clients + 1
$common = "--bot --vsync 0 --fps 120 --size 640 360 --exit-tick $Ticks"
if ($Headless) { $common = "--headless $common" }
if ($Scene -ne "") { $common += " --scene `"$((Resolve-Path $Scene).Path)`"" }
if ($Tuning -ne "") { $common += " --tuning `"$((Resolve-Path $Tuning).Path)`"" }
$hostExtra = if ($ClientExe -ne "") { " --any-build" } else { "" }
$procs = @()
$procs += Start-Process -FilePath $exe -PassThru -NoNewWindow `
    -ArgumentList "--host $Port --local --players $players --auto-start $players --match-data $Bodies --seed 12345$hostExtra $common" `
    -RedirectStandardOutput "$logDir\host.txt" -RedirectStandardError "$logDir\host_err.txt"
Start-Sleep -Milliseconds 500
for ($i = 1; $i -le $Clients; $i++) {
    $procs += Start-Process -FilePath $clientPath -PassThru -NoNewWindow `
        -ArgumentList "--join 127.0.0.1:$Port --threads $i $common" `
        -RedirectStandardOutput "$logDir\client$i.txt" -RedirectStandardError "$logDir\client${i}_err.txt"
}

$timeoutMs = [int](($Ticks / 30.0 + 30) * 1000)
foreach ($p in $procs) {
    if (-not $p.WaitForExit($timeoutMs)) { $p.Kill(); Write-Warning "Process $($p.Id) timed out" }
}

$finals = @()
$names = @("host") + (1..$Clients | ForEach-Object { "client$_" })
foreach ($n in $names) {
    $line = Select-String -Path "$logDir\$n.txt" -Pattern "^FINAL" | Select-Object -Last 1
    $text = if ($line) { $line.Line } else { "(no FINAL line)" }
    Write-Host ("{0,-8} {1}" -f $n, $text)
    $finals += $text
}

$unique = @($finals | Sort-Object -Unique)
if ($unique.Count -eq 1 -and $unique[0] -match "desync=0") {
    Write-Host "PASS: $players peers identical after $Ticks ticks" -ForegroundColor Green
    exit 0
}
Write-Host "FAIL: see $logDir" -ForegroundColor Red
exit 1

# 멀티플레이 결정론 테스트: 같은 PC에서 호스트 1 + 클라이언트 N 을 봇 입력으로 돌리고
# 모두 같은 틱에서 같은 상태 해시를 내는지 확인한다.
# 클라이언트 i는 물리 스레드 i개를 쓴다 (호스트는 자동) → 스레드 수가 달라도 결과가 같은지도 확인된다.
#   powershell -ExecutionPolicy Bypass -File tools\mp_test.ps1 [-Clients 1] [-Ticks 900] [-Config Release] [-Scene <file>] [-Tuning <file>]
# 게임 프로젝트의 실행 파일을 시험하려면 -Exe <프로젝트>\build\Release\bin\<이름>.exe
# 다른 컴파일러 빌드와 맞붙이려면 -ClientExe build\ClangCL\bin\sandbox.exe (호스트는 -Exe, 클라이언트는 이것)
# 먼 상대·손실을 흉내 내려면 -Lag <ms> -Jitter <ms> -Loss <%> (모든 프로세스에 --net-lag 등)
# 창 없이(헤드리스) 돌리려면 -Headless (각 실행 파일에 --headless를 붙인다. 입력이 허락하는 만큼 빨리 돈다)
# 디싱크 진단 시험은 -DesyncTest <틱>: 클라이언트 1이 그 틱에 물리를 조금 바꾼다(--desync-test). 모두 디싱크를 알고, 피어마다
#   build\desync에 덤프를 쓰고, tools\desync_diff.ps1이 처음 갈라진 틱으로 그 틱을 짚으면 PASS
# 중간 참가·재접속 시험은 -LateJoin: 클라이언트 1(--name c1)이 -Ticks의 1/4에서 나갔다가 같은 이름으로 다시 들어오고,
#   매치가 시작된 뒤 새 클라이언트(late) 하나가 빈 슬롯으로 들어온다. 끝까지 간 피어가 모두 같은 해시면 PASS
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
    [int]$Lag = 0,      # 네트워크 상태 흉내 (ms, 모든 프로세스: --net-lag)
    [int]$Jitter = 0,   # --net-jitter (ms)
    [double]$Loss = 0,  # --net-loss (%)
    [switch]$Headless,
    [int]$DesyncTest = 0,
    [switch]$LateJoin
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
$maxPlayers = if ($LateJoin) { $players + 1 } else { $players }
$common = "--bot --mute --vsync 0 --fps 120 --size 640 360 --exit-tick $Ticks"
if ($Headless) { $common = "--headless $common" }
if ($Scene -ne "") { $common += " --scene `"$((Resolve-Path $Scene).Path)`"" }
if ($Tuning -ne "") { $common += " --tuning `"$((Resolve-Path $Tuning).Path)`"" }
if ($Lag -gt 0 -or $Jitter -gt 0 -or $Loss -gt 0) { $common += " --net-lag $Lag --net-jitter $Jitter --net-loss $Loss" }
$hostExtra = if ($ClientExe -ne "") { " --any-build" } else { "" }
$procs = @()
$started = Get-Date
$procs += Start-Process -FilePath $exe -PassThru -NoNewWindow `
    -ArgumentList "--host $Port --local --players $maxPlayers --auto-start $players --match-data $Bodies --seed 12345$hostExtra $common" `
    -RedirectStandardOutput "$logDir\host.txt" -RedirectStandardError "$logDir\host_err.txt"
Start-Sleep -Milliseconds 500
$leaver = $null
for ($i = 1; $i -le $Clients; $i++) {
    $extra = if ($DesyncTest -gt 0 -and $i -eq 1) { " --desync-test $DesyncTest" } else { "" }
    $clientArgs = "--join 127.0.0.1:$Port --threads $i $common$extra"
    if ($LateJoin -and $i -eq 1) { $clientArgs = ($clientArgs -replace "--exit-tick $Ticks", "--exit-tick $([int]($Ticks / 4))") + " --name c1" }
    $p = Start-Process -FilePath $clientPath -PassThru -NoNewWindow -ArgumentList $clientArgs `
        -RedirectStandardOutput "$logDir\client$i.txt" -RedirectStandardError "$logDir\client${i}_err.txt"
    if ($LateJoin -and $i -eq 1) { $leaver = $p } else { $procs += $p }
}
if ($LateJoin) {
    # 새 사람: 매치가 시작되고 조금 뒤. 다시 들어오는 사람: 클라이언트 1이 나간 뒤 같은 이름으로
    Start-Sleep -Milliseconds 1500
    $procs += Start-Process -FilePath $clientPath -PassThru -NoNewWindow -ArgumentList "--join 127.0.0.1:$Port --threads 2 $common" `
        -RedirectStandardOutput "$logDir\late.txt" -RedirectStandardError "$logDir\late_err.txt"
    if (-not $leaver.WaitForExit(60000)) { $leaver.Kill() }
    Start-Sleep -Milliseconds 1500 # 다시 켜는 시간: 끊김 알림이 사라져도(-Loss, -Lag) 예전 연결이 조용해져 같은 이름이 슬롯을 받는다
    $procs += Start-Process -FilePath $clientPath -PassThru -NoNewWindow -ArgumentList "--join 127.0.0.1:$Port --threads 1 --name c1 $common" `
        -RedirectStandardOutput "$logDir\rejoin.txt" -RedirectStandardError "$logDir\rejoin_err.txt"
}

$timeoutMs = [int](($Ticks / 30.0 + 30) * 1000 * (1 + ($Lag + $Jitter) / 100.0)) # 지연이 입력 지연보다 크면 매치가 느려진다
foreach ($p in $procs) {
    if (-not $p.WaitForExit($timeoutMs)) { $p.Kill(); Write-Warning "Process $($p.Id) timed out" }
}

$finals = @()
$names = @("host") + (1..$Clients | ForEach-Object { "client$_" })
if ($LateJoin) {
    $names = @($names | Where-Object { $_ -ne "client1" }) + @("late", "rejoin")
    $players = $names.Count
}
foreach ($n in $names) {
    $line = Select-String -Path "$logDir\$n.txt" -Pattern "^FINAL" | Select-Object -Last 1
    $text = if ($line) { $line.Line } else { "(no FINAL line)" }
    Write-Host ("{0,-8} {1}" -f $n, $text)
    $finals += $text
}

if ($DesyncTest -gt 0) {
    # 덤프는 실행 파일의 프로젝트 폴더 build\desync (seed 12345 = 3039)
    $dumpDir = Join-Path (Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $exe))) "desync"
    $dumps = @(Get-ChildItem $dumpDir -Filter "desync_0000000000003039_t*.txt" -ErrorAction SilentlyContinue |
        Where-Object { $_.LastWriteTime -ge $started })
    $desynced = @($finals | Where-Object { $_ -match "desync=1" }).Count
    Write-Host "$desynced of $players peer(s) report a desync, $($dumps.Count) dump(s) in $dumpDir"
    if ($dumps.Count -ge 2) {
        $diff = & powershell -NoProfile -ExecutionPolicy Bypass -File "$PSScriptRoot\desync_diff.ps1" -A $dumps[0].FullName -B $dumps[1].FullName
        $diff | ForEach-Object { Write-Host $_ }
        if ($desynced -eq $players -and ($diff -match "first diverging tick: $DesyncTest ").Count -gt 0) {
            Write-Host "PASS: desync at tick $DesyncTest found by the dumps" -ForegroundColor Green
            exit 0
        }
    }
    Write-Host "FAIL: see $logDir" -ForegroundColor Red
    exit 1
}

$unique = @($finals | Sort-Object -Unique)
if ($unique.Count -eq 1 -and $unique[0] -match "desync=0") {
    Write-Host "PASS: $players peers identical after $Ticks ticks" -ForegroundColor Green
    exit 0
}
Write-Host "FAIL: see $logDir" -ForegroundColor Red
exit 1

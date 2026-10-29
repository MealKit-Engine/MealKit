# Multiplayer determinism test: runs host 1 + N clients with bot input on the same PC
# and checks they all produce the same state hash at the same ticks.
# Client i uses i physics threads (the host is automatic) → also checks results match across thread counts.
#   powershell -ExecutionPolicy Bypass -File tools\mp_test.ps1 [-Clients 1] [-Ticks 900] [-Config Release] [-Scene <file>] [-Tuning <file>]
# To test a game project's executable: -Exe <project>\build\Release\bin\<name>.exe
# To pit against a build from another compiler: -ClientExe build\ClangCL\bin\sandbox.exe (host is -Exe, clients use this)
# To simulate distant peers/loss: -Lag <ms> -Jitter <ms> -Loss <%> (--net-lag etc. on all processes)
# To run without windows (headless): -Headless (adds --headless to each executable. Runs as fast as input allows)
# Desync diagnostics test with -DesyncTest <tick>: client 1 perturbs physics slightly at that tick (--desync-test). PASS if everyone detects the desync, each peer
#   writes a dump to build\desync, and tools\desync_diff.ps1 pinpoints that tick as the first diverged tick
# Late join/reconnect test with -LateJoin: client 1 (--name c1) leaves at 1/4 of -Ticks and rejoins with the same name,
#   and after the match starts a new client (late) joins an empty slot. PASS if all peers that reached the end have the same hash
# Host migration test with -HostLeave (2+ clients): the host leaves normally at 1/4 of -Ticks (Handoff), and the successor
#   client crashes at 1/2 (--abort-host-at, only whoever is host then). PASS if all remaining clients have the same hash to the end and it migrated twice
param(
    [int]$Clients = 1,
    [int]$Ticks = 900,
    [int]$Bodies = 0, # 0 = box count decided by the scene
    [int]$Port = 27115,
    [string]$Config = "Release",
    [string]$Exe = "",
    [string]$ClientExe = "",
    [string]$Scene = "",
    [string]$Tuning = "",
    [int]$Lag = 0,      # network condition simulation (ms, all processes: --net-lag)
    [int]$Jitter = 0,   # --net-jitter (ms)
    [double]$Loss = 0,  # --net-loss (%)
    [switch]$Headless,
    [int]$DesyncTest = 0,
    [switch]$LateJoin,
    [switch]$HostLeave
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
# Logs: <build folder>\mp_test (executables are in <build>\<Config>\bin)
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
if ($HostLeave -and $Clients -lt 2) { Write-Error "-HostLeave needs -Clients 2 or more"; exit 1 }
$procs = @()
$started = Get-Date
$hostArgs = "--host $Port --local --players $maxPlayers --auto-start $players --match-data $Bodies --seed 12345$hostExtra $common"
if ($HostLeave) { $hostArgs = $hostArgs -replace "--exit-tick $Ticks", "--exit-tick $([int]($Ticks / 4))" }
$procs += Start-Process -FilePath $exe -PassThru -NoNewWindow `
    -ArgumentList $hostArgs `
    -RedirectStandardOutput "$logDir\host.txt" -RedirectStandardError "$logDir\host_err.txt"
Start-Sleep -Milliseconds 500
$leaver = $null
for ($i = 1; $i -le $Clients; $i++) {
    $extra = if ($DesyncTest -gt 0 -and $i -eq 1) { " --desync-test $DesyncTest" } else { "" }
    $clientArgs = "--join 127.0.0.1:$Port --threads $i $common$extra"
    if ($LateJoin -and $i -eq 1) { $clientArgs = ($clientArgs -replace "--exit-tick $Ticks", "--exit-tick $([int]($Ticks / 4))") + " --name c1" }
    if ($HostLeave) { $clientArgs += " --abort-host-at $([int]($Ticks / 2))" }
    $p = Start-Process -FilePath $clientPath -PassThru -NoNewWindow -ArgumentList $clientArgs `
        -RedirectStandardOutput "$logDir\client$i.txt" -RedirectStandardError "$logDir\client${i}_err.txt"
    if ($LateJoin -and $i -eq 1) { $leaver = $p } else { $procs += $p }
}
if ($LateJoin) {
    # Newcomer: shortly after the match starts. Rejoiner: with the same name after client 1 leaves
    Start-Sleep -Milliseconds 1500
    $procs += Start-Process -FilePath $clientPath -PassThru -NoNewWindow -ArgumentList "--join 127.0.0.1:$Port --threads 2 $common" `
        -RedirectStandardOutput "$logDir\late.txt" -RedirectStandardError "$logDir\late_err.txt"
    if (-not $leaver.WaitForExit(60000)) { $leaver.Kill() }
    Start-Sleep -Milliseconds 1500 # restart time: even if the disconnect notice is lost (-Loss, -Lag), the old connection goes quiet so the same name gets the slot
    $procs += Start-Process -FilePath $clientPath -PassThru -NoNewWindow -ArgumentList "--join 127.0.0.1:$Port --threads 1 --name c1 $common" `
        -RedirectStandardOutput "$logDir\rejoin.txt" -RedirectStandardError "$logDir\rejoin_err.txt"
}

$timeoutMs = [int](($Ticks / 30.0 + 30) * 1000 * (1 + ($Lag + $Jitter) / 100.0)) # if the lag exceeds the input delay, the match slows down
foreach ($p in $procs) {
    if (-not $p.WaitForExit($timeoutMs)) { $p.Kill(); Write-Warning "Process $($p.Id) timed out" }
}

$finals = @()
$names = @("host") + (1..$Clients | ForEach-Object { "client$_" })
if ($LateJoin) {
    $names = @($names | Where-Object { $_ -ne "client1" }) + @("late", "rejoin")
    $players = $names.Count
}
if ($HostLeave) { # the host that left and the crashed successor (who it is depends on connection order) don't reach the end
    $names = @($names | Where-Object { $_ -ne "host" -and -not (Select-String -Path "$logDir\${_}_err.txt" -Pattern "--abort-host-at" -Quiet) })
    $players = $names.Count
    $took = @(Select-String -Path ($names | ForEach-Object { "$logDir\${_}_err.txt" }) -Pattern "takes over the session").Count
    Write-Host "$took host migration log line(s) in the remaining clients"
    if ($took -lt 2 * $players) { Write-Host "FAIL: expected two host migrations in every remaining client, see $logDir" -ForegroundColor Red; exit 1 }
}
foreach ($n in $names) {
    $line = Select-String -Path "$logDir\$n.txt" -Pattern "^FINAL" | Select-Object -Last 1
    $text = if ($line) { $line.Line } else { "(no FINAL line)" }
    Write-Host ("{0,-8} {1}" -f $n, $text)
    $finals += $text
}

if ($DesyncTest -gt 0) {
    # Dumps go to build\desync in the executable's project folder (seed 12345 = 3039)
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

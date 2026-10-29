# Live connection check and example client (contract between tools, docs/CODING_MODEL.md "Tools talking to a running editor/game").
#   powershell -ExecutionPolicy Bypass -File tools\live_probe.ps1 [-Project <folder>] [-Kind game|editor] [-Wait <seconds>] [-Events <count>] [-Topics stats]
# 1) reads <project>\build\live\*.json (connection info) → 2) GET <url>api/hello → 3) with -Events N, receives N from GET <url>api/events?topics=....
# Output: per running instance "live: <kind> pid <pid> port <port> project <name> engine <version>", events as "event <topic>: <JSON>".
# MealKit tools in other languages attach in the same order. Exit code 1 if there are none or hello fails.
param(
    [string]$Project = "",
    [string]$Kind = "",
    [double]$Wait = 0,
    [int]$Events = 0,
    [string]$Topics = "stats"
)
$ErrorActionPreference = "Stop"
$engine = (Resolve-Path (Split-Path -Parent $PSScriptRoot)).Path
if ($Project -eq "") { $Project = $engine }
$Project = (Resolve-Path $Project).Path.TrimEnd("\")
$dir = Join-Path $Project "build\live"

# Connection info files (-Wait: waits until they appear. Right after launching a game/editor)
$deadline = (Get-Date).AddSeconds($Wait)
do {
    $infos = @()
    if (Test-Path $dir) {
        foreach ($f in Get-ChildItem $dir -Filter *.json) {
            try { $infos += (Get-Content $f.FullName -Raw -Encoding UTF8 | ConvertFrom-Json) } catch {}
        }
    }
    if ($Kind -ne "") { $infos = @($infos | Where-Object { $_.kind -eq $Kind }) }
    if ($infos.Count -gt 0 -or (Get-Date) -ge $deadline) { break }
    Start-Sleep -Milliseconds 200
} while ($true)
if ($infos.Count -eq 0) {
    Write-Output "FAIL live: nothing running for $Project (no build\live\*.json$(if ($Kind) { " of kind $Kind" }))"
    exit 1
}

$failed = $false
foreach ($info in $infos) {
    try {
        $hello = Invoke-RestMethod -Uri ($info.url + "api/hello") -TimeoutSec 5
    } catch {
        # May be a file left by an abnormal exit (deleted when a server next opens)
        Write-Output "warning: live: $($info.kind) pid $($info.pid) does not answer ($($_.Exception.Message))"
        $failed = $true
        continue
    }
    if ($hello.protocol -ne $info.protocol) {
        Write-Output "error: live: protocol $($hello.protocol) in hello, $($info.protocol) in the info file"
        $failed = $true
    }
    Write-Output "live: $($hello.kind) pid $($hello.pid) port $($info.port) project $($hello.project) engine $($hello.engine) services [$($hello.services -join ', ')] topics [$($hello.topics -join ', ')]"

    if ($Events -gt 0) {
        # Status stream (Server-Sent Events): an "event: <topic>" line + a "data: <JSON>" line + an empty line
        $request = [System.Net.HttpWebRequest]::Create($info.url + "api/events?topics=" + $Topics)
        $request.Timeout = 10000
        $request.ReadWriteTimeout = 10000
        $response = $request.GetResponse()
        $reader = New-Object System.IO.StreamReader($response.GetResponseStream())
        $got = 0
        $topic = ""
        try {
            while ($got -lt $Events) {
                $line = $reader.ReadLine()
                if ($null -eq $line) { break }
                if ($line.StartsWith("event: ")) { $topic = $line.Substring(7) }
                elseif ($line.StartsWith("data: ")) {
                    $null = $line.Substring(6) | ConvertFrom-Json # check that it's JSON
                    Write-Output "event ${topic}: $($line.Substring(6))"
                    $got++
                }
            }
        } finally {
            $reader.Close()
            $response.Close()
        }
        if ($got -lt $Events) {
            Write-Output "error: live: got $got of $Events event(s)"
            $failed = $true
        }
    }
}
if ($failed) { exit 1 }
exit 0

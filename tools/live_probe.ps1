# 라이브 연결 확인과 예시 클라이언트 (도구 사이 약속, docs/CODING_MODEL.md "도구가 실행 중인 에디터·게임과 이야기하기").
#   powershell -ExecutionPolicy Bypass -File tools\live_probe.ps1 [-Project <폴더>] [-Kind game|editor] [-Wait <초>] [-Events <개수>] [-Topics stats]
# 1) <프로젝트>\build\live\*.json (접속 정보)을 읽는다 → 2) GET <url>api/hello → 3) -Events N이면 GET <url>api/events?topics=...에서 N개를 받는다.
# 출력: 실행 중인 것마다 "live: <종류> pid <pid> port <port> project <이름> engine <버전>", 이벤트는 "event <주제>: <JSON>".
# 다른 언어의 MealKit 도구도 같은 순서로 붙는다. 하나도 없거나 hello가 실패하면 종료 코드 1.
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

# 접속 정보 파일 (-Wait: 생길 때까지 기다린다. 게임·에디터를 막 띄웠을 때)
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
        # 비정상 종료로 남은 파일일 수 있다 (다음에 서버가 열릴 때 지운다)
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
        # 상태 스트림 (Server-Sent Events): "event: <주제>" 줄 + "data: <JSON>" 줄 + 빈 줄
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
                    $null = $line.Substring(6) | ConvertFrom-Json # JSON인지 확인
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

# 디싱크 덤프 두 개를 비교한다 (docs/ROADMAP.md 3단계 "디싱크 진단 도구", 형식은 engine/src/mealkit/game/DesyncDump.h).
#   powershell -File tools\desync_diff.ps1 -A build\desync\desync_..._p1.txt -B build\desync\desync_..._p2.txt [-Max 20]
#   powershell -File tools\desync_diff.ps1 -Folder build\desync      같은 매치·틱의 파일을 모두 첫 파일(보통 호스트)과 비교
# 처음 해시가 다른 틱, 값이 다른 상태 필드, 다른 바디(속성마다)를 보여 준다. 종료 코드: 0 같음, 1 다름, 2 읽기 실패.
param(
    [string]$A = "",
    [string]$B = "",
    [string]$Folder = "",
    [int]$Max = 20
)
$ErrorActionPreference = "Stop"

function Read-Dump([string]$path) {
    # 순서 있는 표의 키는 글자로 둔다 (정수로 찾으면 키가 아니라 위치로 찾는다)
    $d = @{ Path = $path; Head = [ordered]@{}; Hashes = [ordered]@{}; State = [ordered]@{}; Bodies = [ordered]@{} }
    $section = "head"
    $format = $false
    foreach ($line in [IO.File]::ReadAllLines($path)) {
        if ($line -eq "" -or $line.StartsWith("#")) { continue }
        if ($line.StartsWith("@format")) {
            if ($line.Trim() -notmatch '^@format desync (\d+)$') { throw "$path is not a desync dump ($line)" }
            if ([int]$Matches[1] -gt 1) { Write-Output "warning: $path is a newer format (version $($Matches[1]))" }
            $format = $true
            continue
        }
        if ($line -match '^\[(\w+)\]$') { $section = $Matches[1]; continue }
        switch ($section) {
            "head" { $i = $line.IndexOf(" "); if ($i -gt 0) { $d.Head[$line.Substring(0, $i)] = $line.Substring($i + 1) } }
            "hashes" { $p = $line.Split(" "); $d.Hashes[$p[0]] = $p[1] }
            "state" { $i = $line.IndexOf(" = "); if ($i -gt 0) { $d.State[$line.Substring(0, $i)] = $line.Substring($i + 3) } }
            "bodies" { $p = $line.Split(" ", 4); $d.Bodies[$p[1]] = @{ Id = $p[2]; Rest = $p[3] } }
        }
    }
    if (-not $format) { throw "$path has no '@format desync' line" }
    return $d
}

# 바디 줄 "p x y z q x y z w v x y z w x y z" → 속성별 값
function Split-Body([string]$rest) {
    $parts = [ordered]@{}
    $name = ""
    foreach ($t in $rest.Split(" ")) {
        if ($t -match '^[a-z]$') { $name = $t; $parts[$name] = @() } else { $parts[$name] += $t }
    }
    return $parts
}

function Compare-Dumps($a, $b) {
    $labelA = "player $($a.Head['player'])"
    $labelB = "player $($b.Head['player'])"
    Write-Output "== $(Split-Path -Leaf $a.Path)  vs  $(Split-Path -Leaf $b.Path)"
    $different = $false
    foreach ($key in @("seed", "content", "tick")) {
        if ($a.Head[$key] -ne $b.Head[$key]) {
            Write-Output "  $key differs: $($a.Head[$key]) vs $($b.Head[$key])$(if ($key -ne 'tick') { ' (not the same match or data)' })"
            $different = $true
        }
    }
    foreach ($d in @($a, $b)) { if ($d.Head.Contains("note")) { Write-Output "  note (player $($d.Head['player'])): $($d.Head['note'])" } }

    # 처음 다른 틱 (두 파일에 다 있는 틱 중)
    $first = $null
    foreach ($t in $a.Hashes.Keys) {
        if ($b.Hashes.Contains($t) -and $a.Hashes[$t] -ne $b.Hashes[$t]) { $first = $t; break }
    }
    if ($null -ne $first) {
        Write-Output "  first diverging tick: $first ($labelA $($a.Hashes[$first]), $labelB $($b.Hashes[$first]))"
        $different = $true
    } else {
        Write-Output "  tick hashes: no difference in the $(@($a.Hashes.Keys | Where-Object { $b.Hashes.Contains($_) }).Count) common tick(s)"
    }

    # 상태 필드
    $fields = @()
    foreach ($k in $a.State.Keys) {
        if (-not $b.State.Contains($k)) { $fields += "  $k = $($a.State[$k])  (only in $labelA)" }
        elseif ($a.State[$k] -ne $b.State[$k]) { $fields += "  $k = $($a.State[$k])  vs  $($b.State[$k])" }
    }
    foreach ($k in $b.State.Keys) { if (-not $a.State.Contains($k)) { $fields += "  $k = $($b.State[$k])  (only in $labelB)" } }
    Write-Output "  state fields that differ at tick $($a.Head['tick']): $($fields.Count)"
    $fields | Select-Object -First $Max | ForEach-Object { Write-Output "  $_" }
    if ($fields.Count -gt $Max) { Write-Output "    ... $($fields.Count - $Max) more" }
    if ($fields.Count -gt 0) { $different = $true }

    # 바디 (생성 순번이 같은 것끼리)
    $bodies = @()
    if ($a.Bodies.Count -ne $b.Bodies.Count) {
        $bodies += "  body count: $($a.Bodies.Count) vs $($b.Bodies.Count)"
    }
    foreach ($i in $a.Bodies.Keys) {
        if (-not $b.Bodies.Contains($i)) { continue }
        $x = $a.Bodies[$i]
        $y = $b.Bodies[$i]
        if ($x.Id -ne $y.Id) { $bodies += "  body $i id $($x.Id) vs $($y.Id)"; continue }
        if ($x.Rest -eq $y.Rest) { continue }
        $px = Split-Body $x.Rest
        $py = Split-Body $y.Rest
        $what = @()
        foreach ($n in $px.Keys) {
            if (($px[$n] -join " ") -ne ($py[$n] -join " ")) { $what += "$n ($($px[$n] -join ' ') vs $($py[$n] -join ' '))" }
        }
        $bodies += "  body $i [$($x.Id)]: $($what -join ', ')"
    }
    Write-Output "  bodies that differ at tick $($a.Head['tick']): $($bodies.Count) of $($a.Bodies.Count)"
    $bodies | Select-Object -First $Max | ForEach-Object { Write-Output "  $_" }
    if ($bodies.Count -gt $Max) { Write-Output "    ... $($bodies.Count - $Max) more" }
    if ($bodies.Count -gt 0) { $different = $true }
    if ($different) { $script:any = $true } # 함수의 Write-Output은 출력이라 돌려주는 값으로 쓰지 않는다
}

try {
    $pairs = @()
    if ($Folder -ne "") {
        $files = @(Get-ChildItem $Folder -Filter "desync_*.txt" | Sort-Object Name)
        if ($files.Count -lt 2) { Write-Output "Need at least two dumps in $Folder"; exit 2 }
        # 같은 매치(시드)·틱끼리 묶어 첫 파일과 나머지를 비교한다
        foreach ($group in ($files | Group-Object { $_.Name -replace '_p\d+\.txt$', '' })) {
            $g = @($group.Group)
            for ($i = 1; $i -lt $g.Count; $i++) { $pairs += , @($g[0].FullName, $g[$i].FullName) }
        }
        if ($pairs.Count -eq 0) { Write-Output "No two dumps of the same match and tick in $Folder"; exit 2 }
    } elseif ($A -ne "" -and $B -ne "") {
        $pairs += , @($A, $B)
    } else {
        Write-Output "Usage: desync_diff.ps1 -A <dump> -B <dump> | -Folder <folder> [-Max 20]"
        exit 2
    }
    $script:any = $false
    foreach ($p in $pairs) { Compare-Dumps (Read-Dump $p[0]) (Read-Dump $p[1]) }
    if ($script:any) { exit 1 }
    Write-Output "SAME"
    exit 0
} catch {
    Write-Output "ERROR: $($_.Exception.Message)"
    exit 2
}

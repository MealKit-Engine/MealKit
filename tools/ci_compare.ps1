# CI 비교: 모든 빌드의 tools/ci_test.ps1 결과를 모아 detmath-bits와 틱별 해시 로그가 같은지 본다.
#   pwsh tools/ci_compare.ps1 -Dir ci-results [-Reference windows-msvc]
# -Dir 아래 빌드마다 폴더 하나(ci_test.ps1의 -Out). 기준 빌드와 다르면 처음 갈라진 틱을 보여 주고 종료 코드 1.
# GitHub Actions에서는 결과 표를 작업 요약($GITHUB_STEP_SUMMARY)에도 쓴다.
param(
    [Parameter(Mandatory = $true)][string]$Dir,
    [string]$Reference = "windows-msvc",
    [string[]]$Expect = @() # 있어야 하는 빌드 이름 (빌드가 실패해 결과가 없으면 비교도 실패)
)
$ErrorActionPreference = "Stop"
$builds = @(Get-ChildItem -Path $Dir -Directory | Sort-Object Name)
if ($builds.Count -eq 0) { Write-Host "No build results in $Dir" -ForegroundColor Red; exit 1 }
$ref = $builds | Where-Object { $_.Name -eq $Reference } | Select-Object -First 1
if (-not $ref) { Write-Host "Reference build '$Reference' missing in $Dir" -ForegroundColor Red; exit 1 }

function Read-Bits($build) {
    $f = Join-Path $build.FullName "detmath-bits.txt"
    if (Test-Path $f) { return (Get-Content $f -Raw).Trim() }
    return $null
}
# "<tick> <hash>" 줄 목록. 처음 다른 줄의 틱을 돌려준다 (같으면 $null)
function First-Difference([string]$a, [string]$b) {
    $la = @(Get-Content $a)
    $lb = @(Get-Content $b)
    $n = [Math]::Max($la.Count, $lb.Count)
    for ($i = 0; $i -lt $n; $i++) {
        $x = if ($i -lt $la.Count) { $la[$i] } else { "(end)" }
        $y = if ($i -lt $lb.Count) { $lb[$i] } else { "(end)" }
        if ($x -ne $y) { return "line $($i + 1): ref '$x' vs '$y'" }
    }
    return $null
}

$refBits = Read-Bits $ref
$refLogs = @(Get-ChildItem -Path $ref.FullName -Filter *.hashes.txt | Sort-Object Name)
if (-not $refBits) { Write-Host "Reference build has no detmath-bits" -ForegroundColor Red; exit 1 }
if ($refLogs.Count -eq 0) { Write-Host "Reference build has no replay hash logs" -ForegroundColor Red; exit 1 }

$rows = @()
$failed = $false
foreach ($e in $Expect) {
    if (-not ($builds | Where-Object { $_.Name -eq $e })) {
        $rows += [pscustomobject]@{ Build = $e; Bits = "missing"; Replay = "**no results (build or test failed)**" }
        $failed = $true
    }
}
foreach ($b in $builds) {
    $bits = Read-Bits $b
    $bitsText = if (-not $bits) { "missing" } elseif ($bits -eq $refBits) { "same" } else { "**$bits**" }
    if ($bits -ne $refBits) { $failed = $true }
    $logs = @(Get-ChildItem -Path $b.FullName -Filter *.hashes.txt)
    $replayText = @()
    if ($logs.Count -eq 0) {
        $replayText += "-" # Editor 빌드 등 헤드리스 타깃이 없는 빌드
    } else {
        foreach ($r in $refLogs) {
            $mine = Join-Path $b.FullName $r.Name
            $id = $r.Name -replace '\.hashes\.txt$', ''
            if (-not (Test-Path $mine)) { $replayText += "${id}: missing"; $failed = $true; continue }
            $diff = First-Difference $r.FullName $mine
            if ($diff) { $replayText += "${id}: **DIFF** $diff"; $failed = $true }
            else { $replayText += "${id}: same ($(@(Get-Content $mine).Count) ticks)" }
        }
    }
    $rows += [pscustomobject]@{ Build = $b.Name; Bits = $bitsText; Replay = ($replayText -join "<br>") }
}

$md = @("### MealKit determinism across builds", "", "Reference: ``$Reference``, detmath-bits ``$refBits``", "",
    "| Build | detmath-bits | Replay tick hashes |", "|---|---|---|")
foreach ($r in $rows) { $md += "| $($r.Build) | $($r.Bits) | $($r.Replay) |" }
$md += ""
$md += if ($failed) { "**FAIL**: builds differ" } else { "**PASS**: all builds identical" }
$md | ForEach-Object { Write-Host $_ }
if ($env:GITHUB_STEP_SUMMARY) { $md | Out-File -Append -Encoding utf8 $env:GITHUB_STEP_SUMMARY }
if ($failed) { exit 1 }
exit 0

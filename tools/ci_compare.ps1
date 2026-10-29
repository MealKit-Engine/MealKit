# CI comparison: collects tools/ci_test.ps1 results of all builds and checks that detmath-bits and per-tick hash logs match.
#   pwsh tools/ci_compare.ps1 -Dir ci-results [-Reference windows-msvc]
# One folder per build under -Dir (ci_test.ps1's -Out). If one differs from the reference build, shows the first diverged tick and exits with code 1.
# On GitHub Actions the result table is also written to the job summary ($GITHUB_STEP_SUMMARY).
param(
    [Parameter(Mandatory = $true)][string]$Dir,
    [string]$Reference = "windows-msvc",
    [string[]]$Expect = @() # build names that must be present (if a build failed and has no results, the comparison fails too)
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
# List of "<tick> <hash>" lines. Returns the tick of the first differing line ($null if identical)
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
        $replayText += "-" # builds without a headless target, like the Editor build
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

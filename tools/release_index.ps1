# Adds an engine package (tools\package_engine.ps1) to the suite release index MealKit Manager reads (ROADMAP R2, docs/DECISIONS.md "How the Manager installs"):
#   tools\release_index.ps1 -Index <index.json> -Zip <mealkit-engine-*.zip> -Url <where the zip is downloaded from>
# The index is created if missing. Only the newest patch of each engine MINOR is kept: adding 0.9.5 drops 0.9.4.
# Format (format 1):
# A MealKit Manager package (mealkit-manager-*.zip) sets the one "manager" entry instead.
#   { "format": 1, "manager": { "version": ..., "url": ..., "size": ..., "sha256": ... }, "engines": [ { "version": "0.9.4", "url": "...", "size": 39366595, "sha256": "..." } ], "tools": [] }
param(
    [Parameter(Mandatory = $true)][string]$Index,
    [Parameter(Mandatory = $true)][string]$Zip,
    [Parameter(Mandatory = $true)][string]$Url
)
$ErrorActionPreference = "Stop"
if (-not (Test-Path $Zip)) { Write-Host "[MealKit] $Zip not found"; exit 1 }
$name = Split-Path $Zip -Leaf
$engines = @()
$tools = @()
$manager = $null
if (Test-Path $Index) {
    $old = Get-Content $Index -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($old.format -ne 1) { Write-Host "[MealKit] $Index has format $($old.format), this script writes 1"; exit 1 }
    if ($old.engines) { $engines = @($old.engines) }
    if ($old.tools) { $tools = @($old.tools) }
    if ($old.manager) { $manager = $old.manager }
}
function Save {
    $o = [ordered]@{ format = 1 }
    if ($manager) { $o.manager = $manager }
    $o.engines = $engines
    $o.tools = $tools
    $json = [pscustomobject]$o | ConvertTo-Json -Depth 6
    [IO.File]::WriteAllText([IO.Path]::GetFullPath($Index), $json + "`n", (New-Object Text.UTF8Encoding $false))
}
function Entry([string]$v) {
    return [pscustomobject]@{ version = $v; url = $Url; size = (Get-Item $Zip).Length; sha256 = (Get-FileHash $Zip -Algorithm SHA256).Hash.ToLower() }
}

# MealKit Manager package (its scripts\package.ps1): one entry, the newest; the Manager's update check reads it
if ($name -match '^mealkit-manager-(\d+\.\d+\.\d+)-windows-x64\.zip$') {
    $v = $Matches[1]
    if ($manager -and [version]$manager.version -gt [version]$v) { Write-Host "[MealKit] $Index already has Manager $($manager.version), newer than $v"; exit 1 }
    $manager = Entry $v
    Save
    Write-Host "[MealKit] $Index`: manager $v"
    exit 0
}
if ($name -notmatch '^mealkit-engine-(\d+)\.(\d+)\.(\d+)-windows-x64\.zip$') { Write-Host "[MealKit] not an engine or Manager package name: $name"; exit 1 }
$version = "$($Matches[1]).$($Matches[2]).$($Matches[3])"
$minor = "$($Matches[1]).$($Matches[2])"
foreach ($e in $engines) {
    if ($e.version.StartsWith("$minor.") -and [version]$e.version -gt [version]$version) {
        Write-Host "[MealKit] $Index already has $($e.version), newer than $version"; exit 1
    }
}
$engines = @($engines | Where-Object { -not $_.version.StartsWith("$minor.") })
$engines += Entry $version
$engines = @($engines | Sort-Object { [version]$_.version } -Descending)
Save
Write-Host "[MealKit] $Index`: engine $version ($($engines.Count) engine versions listed)"
exit 0

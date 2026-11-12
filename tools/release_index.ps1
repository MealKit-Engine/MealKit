# Adds an engine package (tools\package_engine.ps1) to the suite release index MealKit Manager reads (ROADMAP R2, docs/DECISIONS.md "How the Manager installs"):
#   tools\release_index.ps1 -Index <index.json> -Zip <mealkit-engine-*.zip> -Url <where the zip is downloaded from>
# The index is created if missing. Only the newest patch of each engine MINOR is kept: adding 0.9.5 drops 0.9.4.
# Format (format 1):
# A MealKit Manager package (mealkit-manager-*.zip) sets the one "manager" entry instead; a tool package (mealkit-tool-<name>-*.zip,
# tools\package_tool.ps1, needs -Engines) sets that tool's entry in "tools": { "name", "version", "url", "size", "sha256", "protocol", "engines": ["0.9"] }.
#   { "format": 1, "manager": { "version": ..., "url": ..., "size": ..., "sha256": ... }, "engines": [ { "version": "0.9.4", "url": "...", "size": 39366595, "sha256": "..." } ], "tools": [] }
param(
    [Parameter(Mandatory = $true)][string]$Index,
    [Parameter(Mandatory = $true)][string]$Zip,
    [Parameter(Mandatory = $true)][string]$Url,
    [Alias("Engines")][string]$ForEngines = "" # -Engines, tool packages only: engine MINORs it works with ("0.9" or "0.9,0.10")
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
# Tool package (tools\package_tool.ps1): one entry per tool, the newest, with its live protocol (from its mealkit-tool.txt) and
# the engine MINORs it works with (-Engines "0.9,0.10"): the Manager offers it only next to a matching engine
if ($name -match '^mealkit-tool-([A-Za-z][A-Za-z0-9_-]*)-(\d+\.\d+\.\d+)-windows-x64\.zip$') {
    $toolName = $Matches[1]
    $v = $Matches[2]
    $minors = @($ForEngines -split '[,\s]+' | Where-Object { $_ })
    if (-not $minors.Count -or ($minors | Where-Object { $_ -notmatch '^\d+\.\d+$' })) { Write-Host "[MealKit] a tool needs -Engines with engine MINORs, like -Engines 0.9"; exit 1 }
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $archive = [IO.Compression.ZipFile]::OpenRead((Resolve-Path $Zip).Path)
    try {
        $item = $archive.GetEntry("mealkit-tool.txt")
        if (-not $item) { Write-Host "[MealKit] $name has no mealkit-tool.txt at its top"; exit 1 }
        $reader = New-Object IO.StreamReader($item.Open())
        $text = $reader.ReadToEnd()
        $reader.Close()
    } finally { $archive.Dispose() }
    if ($text -notmatch '(?m)^\s*protocol\s+(\d+)') { Write-Host "[MealKit] its mealkit-tool.txt has no protocol line"; exit 1 }
    $protocol = [int]$Matches[1]
    $existing = $tools | Where-Object { $_.name -eq $toolName } | Select-Object -First 1
    if ($existing -and [version]$existing.version -gt [version]$v) { Write-Host "[MealKit] $Index already has $toolName $($existing.version), newer than $v"; exit 1 }
    $entry = Entry $v
    $entry | Add-Member -NotePropertyName name -NotePropertyValue $toolName
    $entry | Add-Member -NotePropertyName protocol -NotePropertyValue $protocol
    $entry | Add-Member -NotePropertyName engines -NotePropertyValue $minors
    $tools = @($tools | Where-Object { $_.name -ne $toolName }) + $entry
    $tools = @($tools | Sort-Object name)
    Save
    Write-Host "[MealKit] $Index`: tool $toolName $v (protocol $protocol, engines $($minors -join ', '))"
    exit 0
}
if ($name -notmatch '^mealkit-engine-(\d+)\.(\d+)\.(\d+)-windows-x64\.zip$') { Write-Host "[MealKit] not an engine, tool or Manager package name: $name"; exit 1 }
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

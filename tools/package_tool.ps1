# Packs a built MealKit tool (MkAudio, MkRender, MkUI, ...) as a zip MealKit Manager installs (ROADMAP R2, docs/DECISIONS.md "How the Manager installs"):
#   tools\package_tool.ps1 -Folder <the tool's bin folder, holding mealkit-tool.txt> [-Out <folder>]
# The folder is zipped as it is (without .pdb/.ilk); mealkit-tool.txt names the tool, its version and its exe.
# Writes <Out>\mealkit-tool-<name>-<version>-windows-x64.zip and prints its size and SHA-256.
# Publish it with tools\release_index.ps1 -Zip <zip> -Url <address> -Engines <MINORs it works with>.
param(
    [Parameter(Mandatory = $true)][string]$Folder,
    [string]$Out = ""
)
$ErrorActionPreference = "Stop"
$Folder = (Resolve-Path $Folder).Path
if (-not $Out) { $Out = Join-Path (Resolve-Path (Join-Path $PSScriptRoot "..")).Path "build\package" }
$manifest = Join-Path $Folder "mealkit-tool.txt"
if (-not (Test-Path $manifest)) { Write-Host "[MealKit] no mealkit-tool.txt in $Folder"; exit 1 }
$keys = @{}
foreach ($line in Get-Content $manifest -Encoding UTF8) {
    if ($line -match '^\s*(name|version|exe|protocol)\s+(.+?)\s*$') { $keys[$Matches[1]] = $Matches[2] }
}
foreach ($k in @("name", "version", "exe", "protocol")) { if (-not $keys[$k]) { Write-Host "[MealKit] mealkit-tool.txt has no $k line"; exit 1 } }
if ($keys.version -notmatch '^\d+\.\d+\.\d+$') { Write-Host "[MealKit] version '$($keys.version)' isn't x.y.z (a manifest from the source tree, not the build?)"; exit 1 }
if ($keys.name -notmatch '^[A-Za-z][A-Za-z0-9_-]*$') { Write-Host "[MealKit] tool name '$($keys.name)' can't be a file name"; exit 1 }
$exe = $keys.exe
if (-not $exe.EndsWith(".exe")) { $exe += ".exe" }
if (-not (Test-Path (Join-Path $Folder $exe))) { Write-Host "[MealKit] $exe not found in $Folder"; exit 1 }

$base = "mealkit-tool-$($keys.name)-$($keys.version)-windows-x64"
$stage = Join-Path $Out $base
if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
New-Item -ItemType Directory -Force $stage | Out-Null
Get-ChildItem $Folder -Recurse -File | Where-Object { $_.Extension -notin @(".pdb", ".ilk") } | ForEach-Object {
    $rel = $_.FullName.Substring($Folder.Length).TrimStart("\")
    $dst = Join-Path $stage $rel
    New-Item -ItemType Directory -Force (Split-Path $dst -Parent) | Out-Null
    Copy-Item $_.FullName $dst
}
$zip = Join-Path $Out "$base.zip"
if (Test-Path $zip) { Remove-Item -Force $zip }
Add-Type -AssemblyName System.IO.Compression.FileSystem
[IO.Compression.ZipFile]::CreateFromDirectory($stage, $zip, [IO.Compression.CompressionLevel]::Optimal, $false)
Write-Host "[MealKit] $zip"
Write-Host "size $((Get-Item $zip).Length)"
Write-Host "sha256 $((Get-FileHash $zip -Algorithm SHA256).Hash.ToLower())"
exit 0

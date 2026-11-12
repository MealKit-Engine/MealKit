# Packs this engine as one zip that MealKit Manager installs without building (ROADMAP R2, docs/DECISIONS.md 2026-11-12):
#   tools\package_engine.ps1 [-Out <folder>] [-Config Editor]
# Contents: every tracked file except docs/ and .github/ (sources stay so Release/Export builds of a game still compile the engine),
# plus the ready Editor build: build\Editor\bin (MealKitEditor.exe, mealkit.dll), engine\mealkit.lib, tools\*.exe and
# MealKitTargets.cmake with its absolute paths turned relative, so the extracted folder works wherever it lands.
# An INSTALLED file marks the package: tools\build.ps1 then links a game's editor build against it without rebuilding the engine.
# Writes <Out>\mealkit-engine-<VERSION>-windows-x64.zip and prints its name, size and SHA-256 (for the release index.json).
param(
    [string]$Out = "",
    [string]$Config = "Editor"
)
$ErrorActionPreference = "Stop"
$engine = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path.TrimEnd("\")
if (-not $Out) { $Out = Join-Path $engine "build\package" }
$version = (Get-Content (Join-Path $engine "VERSION") -Raw).Trim()
$sdk = Join-Path $engine "build\$Config"
$targets = Join-Path $sdk "MealKitTargets.cmake"
foreach ($f in @($targets, "$sdk\bin\MealKitEditor.exe", "$sdk\bin\mealkit.dll", "$sdk\engine\mealkit.lib")) {
    if (-not (Test-Path $f)) { Write-Host "[MealKit] $f not found. Build $Config first."; exit 1 }
}

$stage = Join-Path $Out "mealkit-engine-$version"
if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
New-Item -ItemType Directory -Force $stage | Out-Null

# 1) Tracked files (the working tree's copies), without the docs
Push-Location $engine
$files = & git ls-files -z
Pop-Location
if ($LASTEXITCODE -ne 0) { Write-Host "[MealKit] git ls-files failed"; exit 1 }
$count = 0
foreach ($rel in ($files -split "`0")) {
    if (-not $rel -or $rel -match '^(docs|\.github)/') { continue }
    $src = Join-Path $engine $rel
    if (-not (Test-Path -LiteralPath $src -PathType Leaf)) { continue } # deleted in the working tree
    $dst = Join-Path $stage $rel
    $dir = Split-Path $dst -Parent
    if (-not (Test-Path -LiteralPath $dir)) { New-Item -ItemType Directory -Force $dir | Out-Null }
    Copy-Item -LiteralPath $src -Destination $dst
    $count++
}

# 2) The Editor build a game DLL links against (no .pdb/.ilk, no test leftovers, no sample game DLLs: the Manager builds those on open)
$binOut = Join-Path $stage "build\$Config\bin"
New-Item -ItemType Directory -Force $binOut, (Join-Path $stage "build\$Config\engine"), (Join-Path $stage "build\$Config\tools") | Out-Null
foreach ($name in @("MealKitEditor.exe", "mealkit.dll")) { Copy-Item (Join-Path $sdk "bin\$name") $binOut }
Get-ChildItem (Join-Path $sdk "bin") -Filter *.dll | Where-Object { $_.Name -ne "mealkit.dll" -and $_.Name -notlike "*_game.dll" } |
    ForEach-Object { Copy-Item $_.FullName $binOut } # runtime DLLs beside the editor (SDL, dxcompiler, ...), if any
Copy-Item (Join-Path $sdk "engine\mealkit.lib") (Join-Path $stage "build\$Config\engine")
Get-ChildItem (Join-Path $sdk "tools") -Filter *.exe | ForEach-Object { Copy-Item $_.FullName (Join-Path $stage "build\$Config\tools") }

# 3) MealKitTargets.cmake: <engine> → path relative to the file (it sits in <engine>\build\<Config>)
$text = [IO.File]::ReadAllText($targets)
$root = $engine.Replace("\", "/")
$text = $text.Replace($root + "/", '${CMAKE_CURRENT_LIST_DIR}/../../')
if ($text.Contains($root)) { Write-Host "[MealKit] MealKitTargets.cmake still has $root after rewriting"; exit 1 }
[IO.File]::WriteAllText((Join-Path $stage "build\$Config\MealKitTargets.cmake"), $text)

# 4) Marker
$commit = (& git -C $engine rev-parse HEAD).Trim()
[IO.File]::WriteAllText((Join-Path $stage "INSTALLED"), "version $version`ncommit $commit`nconfig $Config`n")

$zip = Join-Path $Out "mealkit-engine-$version-windows-x64.zip"
if (Test-Path $zip) { Remove-Item -Force $zip }
Add-Type -AssemblyName System.IO.Compression.FileSystem
[IO.Compression.ZipFile]::CreateFromDirectory($stage, $zip, [IO.Compression.CompressionLevel]::Optimal, $false)
$hash = (Get-FileHash $zip -Algorithm SHA256).Hash.ToLower()
$size = (Get-Item $zip).Length
Write-Host "[MealKit] Packed $count tracked files + Editor build"
Write-Host "[MealKit] $zip"
Write-Host "size $size"
Write-Host "sha256 $hash"
exit 0

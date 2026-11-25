# Converts .fbx files (Mixamo, Cascadeur, 3ds Max) to .glb with Blender, without a window.
#   tools\fbx_to_glb.ps1 -Path art\characters\hero.fbx            -> art\characters\hero.glb next to the source
#   tools\fbx_to_glb.ps1 -Path art\characters -Out assets\models\characters   (every .fbx in the folder)
# Then open the .glb in Blender or the MealKit add-on to check it, or copy it into assets/models/ (docs "Blender add-on").
# Blender: $env:MEALKIT_BLENDER, else the usual install folder, else `blender` on PATH.
param(
    [Parameter(Mandatory = $true)][string]$Path,
    [string]$Out = ""
)
$ErrorActionPreference = "Stop"
$onWindows = $PSVersionTable.PSVersion.Major -lt 6 -or $IsWindows
$blender = $env:MEALKIT_BLENDER
if (-not $blender -and $onWindows) { $blender = "$env:ProgramFiles/Blender Foundation/Blender 5.2/blender.exe" }
if (-not $blender -or -not (Test-Path $blender)) {
    $blender = (Get-Command blender -ErrorAction SilentlyContinue).Source
}
if (-not $blender) { throw "Blender 5.2 not found (set MEALKIT_BLENDER to blender's path)" }

if (-not (Test-Path $Path)) { throw "not found: $Path" }
$item = Get-Item $Path
$files = if ($item.PSIsContainer) { @(Get-ChildItem $item.FullName -Recurse -File -Filter *.fbx) } else { @($item) }
if ($files.Count -eq 0) { throw "no .fbx files in $Path" }
if (-not $Out) { $Out = if ($item.PSIsContainer) { $item.FullName } else { $item.DirectoryName } }
New-Item -ItemType Directory -Force $Out | Out-Null

$script = Join-Path $PSScriptRoot "fbx_to_glb.py"
& $blender -b --factory-startup --python $script -- @($files.FullName) --out (Resolve-Path $Out).Path
if ($LASTEXITCODE -ne 0) { throw "conversion failed (see the Blender output above)" }

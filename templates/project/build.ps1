# @NAME@ build on every OS (Windows: build.bat does the same). The engine folder is the "engine" line in mealkit.project.
#   pwsh build.ps1 [Editor|Release|Debug|Profile|Export]
param([string]$Config = "Release")
$engine = ""
foreach ($line in Get-Content (Join-Path $PSScriptRoot "mealkit.project")) {
    if ($line -match "^\s*engine\s*[= ]\s*(.+?)\s*$") { $engine = $Matches[1] }
}
$script = Join-Path $engine "tools/build.ps1"
if (-not $engine -or -not (Test-Path $script)) {
    Write-Host "[MealKit] Engine not found at `"$engine`" - set the engine line in mealkit.project"
    exit 1
}
& $script -Project $PSScriptRoot -Config $Config
exit $LASTEXITCODE

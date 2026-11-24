# @NAME@ build on every OS (Windows: build.bat does the same). The engine folder is the "engine" line in mealkit.project.
#   pwsh build.ps1 [Editor|Release|Debug|Profile|Export]
param([string]$Config = "Release")
$engine = ""
foreach ($line in Get-Content (Join-Path $PSScriptRoot "mealkit.project")) {
    if ($line -match "^\s*engine\s*[= ]\s*(.+?)\s*$") { $engine = $Matches[1] }
}
if ($engine -and -not [IO.Path]::IsPathRooted($engine) -and $engine -notmatch '^[A-Za-z]:') { $engine = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot $engine)) } # relative = to this folder
# The engine line may be a Windows path (D:\...). Elsewhere: MEALKIT_ROOT, then a MealKit folder beside the project or two levels up (games/<name>).
if (-not $engine -or -not (Test-Path (Join-Path $engine "tools/build.ps1"))) {
    foreach ($candidate in @($env:MEALKIT_ROOT, (Join-Path $PSScriptRoot "../MealKit"), (Join-Path $PSScriptRoot "../../MealKit"))) {
        if ($candidate -and (Test-Path (Join-Path $candidate "tools/build.ps1"))) { $engine = (Resolve-Path $candidate).Path; break }
    }
}
$script = Join-Path $engine "tools/build.ps1"
if (-not $engine -or -not (Test-Path $script)) {
    Write-Host "[MealKit] Engine not found at `"$engine`" - set the engine line in mealkit.project"
    exit 1
}
& $script -Project $PSScriptRoot -Config $Config
exit $LASTEXITCODE

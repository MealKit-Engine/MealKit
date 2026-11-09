# The engine changed while an editor was running: its mealkit.dll is in use, so a game code build can't rebuild the engine.
# The editor starts this (its build failure prompt) and quits: wait for that editor to exit, build the engine's editor, open the project again
# (the reopened editor sees game code older than the engine and builds it).
#   powershell -ExecutionPolicy Bypass -File tools\rebuild_editor.ps1 -Project <project folder> [-WaitPid <editor process id>]
param(
    [Parameter(Mandatory = $true)][string]$Project,
    [int]$WaitPid = 0
)
$ErrorActionPreference = "Stop"
$engine = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$host.UI.RawUI.WindowTitle = "MealKit: rebuilding the editor"

if ($WaitPid -gt 0) {
    Write-Host "[MealKit] Waiting for the editor (process $WaitPid) to close..."
    Wait-Process -Id $WaitPid -Timeout 120 -ErrorAction SilentlyContinue
    if (Get-Process -Id $WaitPid -ErrorAction SilentlyContinue) {
        Write-Host "[MealKit] The editor is still running. Close it and run this again."
        Read-Host "Press Enter to close"
        exit 1
    }
}

Write-Host "[MealKit] Building the engine editor in $engine"
& (Join-Path $PSScriptRoot "build.ps1") -Project $engine -Config Editor
if ($LASTEXITCODE -ne 0) {
    Write-Host "[MealKit] The engine editor build failed (see above)."
    Read-Host "Press Enter to close"
    exit $LASTEXITCODE
}

$editor = Join-Path $engine "build\Editor\bin\MealKitEditor.exe"
Write-Host "[MealKit] Opening $Project"
Start-Process -FilePath $editor -ArgumentList @("--project", "`"$Project`"") -WorkingDirectory $Project

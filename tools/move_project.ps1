# Moves a game project folder. Run when the folder is edited in MealKit Manager's project list (or the editor's project manager).
#   powershell -ExecutionPolicy Bypass -File tools\move_project.ps1 -Project <folder> -To <new folder>
# - To must not exist, or be an empty folder (it is replaced)
# - deletes build\ first (the CMake cache and outputs hold absolute paths; the game is rebuilt when it opens)
# - same drive: a rename; another drive: copy, then delete the original (the copy is removed again if it fails)
# - the engine's own sample projects (inside the engine repo) can't be moved
# - last line on success: moved<TAB><new folder>
# Recent and favorite lists are the caller's job (the Manager and the editor keep them).
param(
    [Parameter(Mandatory = $true)][string]$Project,
    [Parameter(Mandatory = $true)][string]$To
)
$ErrorActionPreference = "Stop"
. "$PSScriptRoot\project_common.ps1"

$Project = (Resolve-Path $Project).Path.TrimEnd("\")
$To = [IO.Path]::GetFullPath($To).TrimEnd("\")
if (-not (Test-Path (Join-Path $Project "mealkit.project"))) {
    Write-Error "$Project has no mealkit.project"
    exit 1
}
$engineRoot = $MealKitEngine.TrimEnd("\") + "\"
if (($Project + "\").StartsWith($engineRoot, [StringComparison]::OrdinalIgnoreCase)) {
    Write-Error "The engine's sample projects cannot be moved"
    exit 1
}
if ($To -ieq $Project) {
    Write-Host "Already at $To"
    Write-Output "moved`t$To"
    exit 0
}
if (($To + "\").StartsWith($Project + "\", [StringComparison]::OrdinalIgnoreCase)) {
    Write-Error "$To is inside the project folder"
    exit 1
}
if (Test-Path -LiteralPath $To) {
    if (Get-ChildItem -LiteralPath $To -Force | Select-Object -First 1) {
        Write-Error "$To already exists and is not empty"
        exit 1
    }
    Remove-Item -LiteralPath $To -Force
}
$parent = Split-Path -Parent $To
if ($parent -and -not (Test-Path -LiteralPath $parent)) { New-Item -ItemType Directory -Path $parent | Out-Null }

$build = Join-Path $Project "build"
if (Test-Path -LiteralPath $build) {
    try { Remove-Item -LiteralPath $build -Recurse -Force }
    catch {
        Write-Error "Could not remove the build folder (is the project open?): $($_.Exception.Message)"
        exit 1
    }
}

if ([IO.Path]::GetPathRoot($Project) -ieq [IO.Path]::GetPathRoot($To)) {
    try { [IO.Directory]::Move($Project, $To) }
    catch {
        Write-Error "Could not move the folder (is a file in it open?): $($_.Exception.Message)"
        exit 1
    }
} else {
    try { Copy-Item -LiteralPath $Project -Destination $To -Recurse -Force }
    catch {
        if (Test-Path -LiteralPath $To) { Remove-Item -LiteralPath $To -Recurse -Force -ErrorAction SilentlyContinue }
        Write-Error "Could not copy the folder: $($_.Exception.Message)"
        exit 1
    }
    try { Remove-Item -LiteralPath $Project -Recurse -Force }
    catch { Write-Warning "Copied to $To, but the original could not be deleted: $($_.Exception.Message)" }
}
Write-Host "Moved $Project -> $To"
Write-Output "moved`t$To"

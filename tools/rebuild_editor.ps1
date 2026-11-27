# The engine changed while an editor was running: its mealkit.dll is in use, so a game code build can't rebuild the engine.
# The editor starts this (its build failure prompt) and quits: wait for that editor to exit, build the engine's editor, open the project again
# (the reopened editor sees game code older than the engine and builds it).
#   powershell -ExecutionPolicy Bypass -File tools\rebuild_editor.ps1 -Project <project folder> [-WaitPid <editor process id>]
# Only one runs per engine. On Linux the editor starts it without a terminal: zenity shows the progress and the failure, and the output
# goes to <user folder>/editor_build.log.
param(
    [Parameter(Mandatory = $true)][string]$Project,
    [int]$WaitPid = 0
)
$ErrorActionPreference = "Stop"
$engine = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
. (Join-Path $PSScriptRoot "build_dirs.ps1")
. (Join-Path $PSScriptRoot "build_lock.ps1")
$onWindows = $MealKitBuildOs -eq "windows"
$dialogs = -not $onWindows -and [Console]::IsOutputRedirected -and ($env:DISPLAY -or $env:WAYLAND_DISPLAY) -and
    [bool](Get-Command zenity -ErrorAction SilentlyContinue)
if (-not [Console]::IsOutputRedirected) { $host.UI.RawUI.WindowTitle = "MealKit: rebuilding the editor" }

function Show-Dialog([string]$kind, [string]$text) {
    if ($dialogs) { & zenity "--$kind" --no-markup "--title=MealKit Editor" "--text=$text" 2>$null }
}
function Stop-Rebuild([string]$message, [int]$code) {
    Write-Host "[MealKit] $message"
    if ($dialogs) { Show-Dialog "error" $message }
    elseif (-not [Console]::IsInputRedirected) { Read-Host "Press Enter to close" }
    exit $code
}

$only = Open-BuildLock (Join-Path (Join-Path (Join-Path $engine "build") $MealKitBuildOs) "rebuild_editor.lock")
if (-not $only) {
    $message = "The editor is already being rebuilt. It opens by itself when the build is done."
    Write-Host "[MealKit] $message"
    Show-Dialog "info" $message
    if (-not $dialogs) { Start-Sleep -Seconds 5 }
    exit 0
}

# An editor that exited but wasn't reaped by the program that started it (a zombie) still counts as a process for Get-Process
function Test-Running([int]$id) {
    if (-not (Get-Process -Id $id -ErrorAction SilentlyContinue)) { return $false }
    $stat = "/proc/$id/stat"
    if ($onWindows -or -not (Test-Path $stat)) { return $true }
    return (Get-Content -Raw $stat -ErrorAction SilentlyContinue) -notmatch '\) [ZX] '
}

if ($WaitPid -gt 0) {
    Write-Host "[MealKit] Waiting for the editor (process $WaitPid) to close..."
    $waited = [Diagnostics.Stopwatch]::StartNew()
    while ((Test-Running $WaitPid) -and $waited.Elapsed.TotalSeconds -lt 120) { Start-Sleep -Milliseconds 200 }
    if (Test-Running $WaitPid) {
        Stop-Rebuild "The editor is still running. Close it and open the project again." 1
    }
}

Write-Host "[MealKit] Building the engine editor in $engine"
$script = Join-Path $PSScriptRoot "build.ps1"
if ($dialogs) {
    $data = if ($env:XDG_DATA_HOME) { $env:XDG_DATA_HOME } else { Join-Path $HOME ".local/share" }
    $log = Join-Path (Join-Path $data "MealKit") "editor_build.log"
    New-Item -ItemType Directory -Force (Split-Path -Parent $log) | Out-Null
    $info = New-Object Diagnostics.ProcessStartInfo "zenity", "--progress --auto-close --no-cancel --width=420 `"--title=MealKit Editor`" `"--text=Building the editor...`""
    $info.UseShellExecute = $false
    $info.RedirectStandardInput = $true
    $progress = [Diagnostics.Process]::Start($info)
    # zenity reads "<percent>" and "# <text>" lines; its window may be closed while the build goes on
    function Send-Progress([string]$line) {
        try { $progress.StandardInput.WriteLine($line); $progress.StandardInput.Flush() } catch {}
    }
    $shell = (Get-Process -Id $PID).Path
    & $shell -NoProfile -File $script -Project $engine -Config Editor 2>&1 | ForEach-Object {
        $line = "$_"
        if ($line -match '^\[(\d+)/(\d+)\]') {
            Send-Progress "$([Math]::Min(99, [Math]::Floor(100 * [int]$Matches[1] / [int]$Matches[2])))"
            Send-Progress "# Building the editor... $($Matches[1])/$($Matches[2])"
        } elseif ($line -like "*waiting for it to finish*") {
            Send-Progress "# Waiting for another build to finish..."
        }
        $line
    } | Set-Content $log
    $code = $LASTEXITCODE
    Send-Progress "100"
    try { $progress.StandardInput.Close() } catch {}
    if ($code -ne 0) { Stop-Rebuild "The engine editor build failed. Log: $log" $code }
} else {
    & $script -Project $engine -Config Editor
    if ($LASTEXITCODE -ne 0) { Stop-Rebuild "The engine editor build failed (see above)." $LASTEXITCODE }
}

$exe = if ($onWindows) { "MealKitEditor.exe" } else { "MealKitEditor" }
$editor = Join-Path (Join-Path (Get-NewBuildDir $engine "Editor") "bin") $exe
$only.Dispose()
Write-Host "[MealKit] Opening $Project"
Start-Process -FilePath $editor -ArgumentList @("--project", "`"$Project`"") -WorkingDirectory $Project

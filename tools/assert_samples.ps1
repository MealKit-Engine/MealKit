# Stops a script early when samples/<name> is missing or differs from samples.lock (tools\sample.ps1 status), like engine modules.
# Dot-sourced by the check, test and recording scripts:   . "$PSScriptRoot\assert_samples.ps1"; Assert-Samples
function Assert-Samples {
    $shell = (Get-Process -Id $PID).Path # the same PowerShell (5.1 or 7)
    $report = & $shell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot "sample.ps1") status 2>&1
    if ($LASTEXITCODE -ne 0) {
        $report | ForEach-Object { Write-Host "$_" }
        Write-Host "samples/ does not match samples.lock: fix it with tools\sample.ps1 sync (or 'sample.ps1 local on' for a checkout you are changing)." -ForegroundColor Red
        exit 1
    }
}

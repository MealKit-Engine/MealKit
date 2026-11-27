# One build at a time per build tree (dot-source: . "$PSScriptRoot/build_lock.ps1"). Two ninja runs in the same tree overwrite each
# other's objects and libraries (malformed archives, "file format not recognized", "failed recompaction").
# A lock is a file beside the tree held open without sharing (Linux/macOS: .NET takes an flock), so it goes away with the process, also
# when a build is cancelled or killed.
#   Open-BuildLock   the held lock, or $null when another process has it
#   Wait-BuildLock   waits for it and returns it
#   .Dispose()       releases it

function Open-BuildLock([string]$path) {
    New-Item -ItemType Directory -Force (Split-Path -Parent $path) | Out-Null
    try {
        return [IO.File]::Open($path, [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
    } catch [IO.IOException] {
        return $null
    }
}

function Wait-BuildLock([string]$buildDir) {
    $path = "$buildDir.lock"
    $lock = Open-BuildLock $path
    if (-not $lock) {
        Write-Host "[MealKit] Another build is using $buildDir; waiting for it to finish..."
        while (-not $lock) {
            Start-Sleep -Seconds 1
            $lock = Open-BuildLock $path
        }
    }
    return $lock
}

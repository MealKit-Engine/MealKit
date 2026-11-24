# Repo and lock helpers shared by tools\module_engine.ps1 (modules.lock) and tools\sample.ps1 (samples.lock). Dot-sourced:
#   . "$PSScriptRoot\lock_repo.ps1"
# Lock lines are "<kind> <name> <repo url> <tag> <commit> [<path in the repo>]"; a missing path means the repo root.
$utf8NoBom = New-Object System.Text.UTF8Encoding $false

function Read-LockFile([string]$path, [string]$kind, [int]$fields) {
    $entries = @()
    foreach ($line in [IO.File]::ReadAllLines($path)) {
        $t = $line.Trim()
        if (-not $t -or $t.StartsWith("#") -or $t.StartsWith("@format")) { continue }
        $p = $t -split '\s+'
        if ($p[0] -ne $kind -or $p.Count -lt $fields) { throw "$(Split-Path -Leaf $path): bad line '$t'" }
        $entries += [pscustomobject]@{ Name = $p[1]; Url = $p[2]; Tag = $p[3]; Commit = $p[4]; Path = $(if ($p.Count -gt 5) { $p[5] } else { "." }) }
    }
    return $entries
}

function Write-LockFile([string]$path, [string[]]$header, [string]$kind, $entries, [bool]$withPath) {
    $lines = @($header)
    foreach ($e in $entries) {
        $line = "$kind $($e.Name) $($e.Url) $($e.Tag) $($e.Commit)"
        if ($withPath) { $line += " $($e.Path)" }
        $lines += $line
    }
    [IO.File]::WriteAllText($path, (($lines -join "`r`n") + "`r`n"), $utf8NoBom)
}

function Repo-Name($url) { return ($url.TrimEnd("/") -split "/")[-1] }

# The repo checkout for an entry: the sibling folder beside the engine, or a clone in the per-user cache ($cacheFolder under MealKit)
function Get-RepoDir($e, [string]$workspace, [string]$cacheFolder) {
    $sibling = Join-Path $workspace (Repo-Name $e.Url)
    if (Test-Path (Join-Path $sibling ".git")) { return $sibling }
    $cacheRoot = if ($env:LOCALAPPDATA) { $env:LOCALAPPDATA } elseif ($env:XDG_CACHE_HOME) { $env:XDG_CACHE_HOME } else { Join-Path $HOME ".cache" }
    $cache = Join-Path $cacheRoot ("MealKit\$cacheFolder\" + (Repo-Name $e.Url))
    if (-not (Test-Path (Join-Path $cache ".git"))) {
        & git clone --quiet $e.Url $cache
        if ($LASTEXITCODE -ne 0) { throw "could not clone $($e.Url)" }
    }
    & git -C $cache fetch --quiet --tags
    return $cache
}

function Git-Lines($dir, [string[]]$arguments) {
    $out = & git -C $dir @arguments
    if ($LASTEXITCODE -ne 0) { throw "git $($arguments -join ' ') failed in $dir" }
    return @($out)
}

# Files at the locked commit below the entry's path: relative path -> blob id. $skip decides which relative paths stay out.
function Get-LockedFiles($e, $dir, [scriptblock]$skip) {
    $files = @{}
    $prefix = if ($e.Path -eq ".") { "" } else { $e.Path.TrimEnd("/") + "/" }
    $spec = if ($prefix) { @($e.Commit, "--", $prefix) } else { @($e.Commit) }
    foreach ($line in Git-Lines $dir (@("ls-tree", "-r") + $spec)) {
        $tab = $line.IndexOf("`t")
        $blob = ($line.Substring(0, $tab) -split ' ')[2]
        $rel = $line.Substring($tab + 1).Substring($prefix.Length)
        if (& $skip $rel) { continue }
        $files[$rel] = $blob
    }
    return $files
}

# Writes one blob's bytes to $dest
function Copy-GitBlob($dir, [string]$blob, [string]$dest) {
    if ($IsLinux -or $IsMacOS) {
        # PowerShell 7 there rewrites redirected output line by line and drops empty lines: copy git's bytes as they are
        $psi = New-Object Diagnostics.ProcessStartInfo "git", "-C `"$dir`" cat-file blob $blob"
        $psi.UseShellExecute = $false
        $psi.RedirectStandardOutput = $true
        $proc = [Diagnostics.Process]::Start($psi)
        $stream = [IO.File]::Create($dest)
        try { $proc.StandardOutput.BaseStream.CopyTo($stream) } finally { $stream.Close() }
        $proc.WaitForExit()
        $code = $proc.ExitCode
    } else {
        $p = Start-Process -FilePath git -ArgumentList @("-C", "`"$dir`"", "cat-file", "blob", $blob) -NoNewWindow -Wait -PassThru -RedirectStandardOutput $dest
        $code = $p.ExitCode
    }
    if ($code -ne 0) { throw "could not read blob $blob from $dir" }
}

function Remove-EmptyDirectories([string]$root) {
    Get-ChildItem -Path $root -Recurse -Directory | Sort-Object { $_.FullName.Length } -Descending |
        Where-Object { -not (Get-ChildItem -LiteralPath $_.FullName -Force) } | Remove-Item -Force
}

function Is-Junction($path) {
    if (-not (Test-Path $path)) { return $false }
    return [bool]((Get-Item $path -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)
}

# Linux/macOS have no junctions (New-Item makes nothing there): a symbolic link, which Is-Junction sees as a reparse point too
function New-DirectoryLink([string]$path, [string]$target) {
    $linkType = if ([Environment]::OSVersion.Platform -eq [PlatformID]::Win32NT) { "Junction" } else { "SymbolicLink" }
    New-Item -ItemType $linkType -Path $path -Target $target | Out-Null
    if (-not (Is-Junction $path)) { throw "could not link $path to $target" }
}

function Newest-Tag($dir) { return (& git -C $dir tag --list "v*" --sort=-v:refname | Select-Object -First 1) }

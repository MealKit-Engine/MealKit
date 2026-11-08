# Engine modules vendored from their own repos (docs/ENGINE_MODULES.md "Lock file and vendored copies").
#   powershell -ExecutionPolicy Bypass -File tools\modules.ps1 status            vendored copy vs modules.lock vs the local checkout
#   powershell -ExecutionPolicy Bypass -File tools\modules.ps1 sync [name ...]   rewrite modules/<name> from the locked commit
#   powershell -ExecutionPolicy Bypass -File tools\modules.ps1 bump <name> [tag] lock a new tag (default: the checkout's newest v* tag) and sync it
#   powershell -ExecutionPolicy Bypass -File tools\modules.ps1 fetch -Project <folder>
#       a game's engineModuleVersion <name> <tag> lines: that tag's files into %LOCALAPPDATA%\MealKit\modules\<name>@<tag> (the build uses them)
#   powershell -ExecutionPolicy Bypass -File tools\modules.ps1 local on|off [name ...]
#       on:  modules/<name> becomes a junction to the sibling checkout ..\MealKit-<Name>, for changes that span repos (the engine build
#            uses the checkout's working files). off: removes the junctions and restores the vendored copies from git.
# Repos are read from sibling checkouts (D:\MealKit-Engine\MealKit-<Name>), cloned from the lock's URL into
# %LOCALAPPDATA%\MealKit\modules when missing. A vendored copy is the repo's files at the locked commit without README.md and .github/;
# local notes (CLAUDE.md) are left alone.
param(
    [Parameter(Mandatory = $true, Position = 0)][ValidateSet("status", "sync", "bump", "local", "fetch")][string]$Command,
    [string]$Project = "",
    [Parameter(Position = 1, ValueFromRemainingArguments = $true)][string[]]$Names = @()
)
$ErrorActionPreference = "Stop"
$engine = Split-Path -Parent $PSScriptRoot
$lockPath = Join-Path $engine "modules.lock"
$workspace = Split-Path -Parent $engine
$utf8 = New-Object System.Text.UTF8Encoding $false

# ---- modules.lock: "module <name> <url> <tag> <commit> <path in repo>"
function Read-Lock {
    $entries = @()
    foreach ($line in [IO.File]::ReadAllLines($lockPath)) {
        $t = $line.Trim()
        if (-not $t -or $t.StartsWith("#") -or $t.StartsWith("@format")) { continue }
        $p = $t -split '\s+'
        if ($p[0] -ne "module" -or $p.Count -lt 6) { throw "modules.lock: bad line '$t'" }
        $entries += [pscustomobject]@{ Name = $p[1]; Url = $p[2]; Tag = $p[3]; Commit = $p[4]; Path = $p[5] }
    }
    return $entries
}

function Write-Lock($entries) {
    $lines = @("# Engine modules vendored into modules/<name> (tools\modules.ps1). One engine version = VERSION + this file.",
               "# module <name> <repo url> <tag> <commit> <path in the repo>",
               "@format module-lock 1")
    foreach ($e in $entries) { $lines += "module $($e.Name) $($e.Url) $($e.Tag) $($e.Commit) $($e.Path)" }
    [IO.File]::WriteAllText($lockPath, (($lines -join "`r`n") + "`r`n"), $utf8)
}

function Repo-Name($url) { return ($url.TrimEnd("/") -split "/")[-1] }

# The repo checkout for an entry: the sibling folder, or a cache clone
function Repo-Dir($e) {
    $sibling = Join-Path $workspace (Repo-Name $e.Url)
    if (Test-Path (Join-Path $sibling ".git")) { return $sibling }
    $cache = Join-Path $env:LOCALAPPDATA ("MealKit\modules\" + (Repo-Name $e.Url))
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

# Files of the vendored copy at the locked commit: relative path → blob id
function Locked-Files($e, $dir) {
    $files = @{}
    $prefix = if ($e.Path -eq ".") { "" } else { $e.Path.TrimEnd("/") + "/" }
    $spec = if ($prefix) { @($e.Commit, "--", $prefix) } else { @($e.Commit) }
    foreach ($line in Git-Lines $dir (@("ls-tree", "-r") + $spec)) {
        $tab = $line.IndexOf("`t")
        $blob = ($line.Substring(0, $tab) -split ' ')[2]
        $rel = $line.Substring($tab + 1).Substring($prefix.Length)
        if ($rel -eq "README.md" -or $rel.StartsWith(".github/") -or $rel.StartsWith("backends/")) { continue }
        $files[$rel] = $blob
    }
    return $files
}

function Vendored-Dir($e) { return Join-Path $engine ("modules\" + $e.Name) }

function Is-Junction($path) {
    if (-not (Test-Path $path)) { return $false }
    return [bool]((Get-Item $path -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)
}

function Vendored-Differences($e, $dir) {
    $locked = Locked-Files $e $dir
    $target = Vendored-Dir $e
    $diff = @()
    foreach ($rel in $locked.Keys) {
        $full = Join-Path $target $rel
        if (-not (Test-Path $full)) { $diff += "missing $rel"; continue }
        $blob = (& git hash-object --no-filters $full).Trim()
        if ($blob -ne $locked[$rel]) {
            $normalized = (& git -C $engine hash-object --path ("modules/$($e.Name)/" + $rel) $full).Trim() # line ending conversion
            if ($normalized -ne $locked[$rel]) { $diff += "changed $rel" }
        }
    }
    if (Test-Path $target) {
        foreach ($f in Get-ChildItem -Path $target -Recurse -File) {
            $rel = $f.FullName.Substring($target.Length + 1).Replace("\", "/")
            if ($f.Name -eq "CLAUDE.md" -or $rel.StartsWith("backends/")) { continue }
            if (-not $locked.ContainsKey($rel)) { $diff += "extra $rel" }
        }
    }
    return $diff
}

function Sync-Entry($e, [string]$into = "") {
    $dir = Repo-Dir $e
    $target = if ($into) { $into } else { Vendored-Dir $e }
    if (-not $into -and (Is-Junction $target)) { throw "modules\$($e.Name) is in local mode; run 'modules.ps1 local off $($e.Name)' first" }
    $locked = Locked-Files $e $dir
    if (Test-Path $target) {
        foreach ($f in Get-ChildItem -Path $target -Recurse -File) {
            if ($f.Name -ne "CLAUDE.md") { Remove-Item -LiteralPath $f.FullName -Force }
        }
    }
    $prefix = if ($e.Path -eq ".") { "" } else { $e.Path.TrimEnd("/") + "/" }
    foreach ($rel in $locked.Keys) {
        $full = Join-Path $target $rel
        New-Item -ItemType Directory -Force (Split-Path -Parent $full) | Out-Null
        $p = Start-Process -FilePath git -ArgumentList @("-C", "`"$dir`"", "cat-file", "blob", $locked[$rel]) -NoNewWindow -Wait -PassThru `
            -RedirectStandardOutput $full
        if ($p.ExitCode -ne 0) { throw "could not read $prefix$rel from $dir" }
    }
    # Empty folders left by removed files
    Get-ChildItem -Path $target -Recurse -Directory | Sort-Object { $_.FullName.Length } -Descending |
        Where-Object { -not (Get-ChildItem -LiteralPath $_.FullName -Force) } | Remove-Item -Force
    Write-Host "synced $($e.Name) $($e.Tag) ($($e.Commit.Substring(0, 9))) -> $target"
}

$entries = Read-Lock
$selected = if ($Names.Count -and $Command -ne "bump" -and $Command -ne "local") { $entries | Where-Object { $Names -contains $_.Name } } else { $entries }

switch ($Command) {
    "status" {
        $bad = 0
        foreach ($e in $selected) {
            $target = Vendored-Dir $e
            if (Is-Junction $target) { Write-Host ("{0,-14} LOCAL  -> {1}" -f $e.Name, (Get-Item $target).Target); continue }
            $dir = Repo-Dir $e
            $diff = Vendored-Differences $e $dir
            $head = @(Git-Lines $dir @("rev-parse", "HEAD"))[0].Trim()
            $newest = (& git -C $dir tag --list "v*" --sort=-v:refname | Select-Object -First 1)
            $note = if ($head -ne $e.Commit) { "checkout at $($head.Substring(0, 9)), newest tag $newest" } else { "" }
            if ($diff.Count) { $bad++; Write-Host ("{0,-14} DIFF   {1} {2}" -f $e.Name, $e.Tag, ($diff -join ", ")) -ForegroundColor Red }
            else { Write-Host ("{0,-14} ok     {1} {2}" -f $e.Name, $e.Tag, $note) }
        }
        if ($bad) { exit 1 }
    }
    "sync" { foreach ($e in $selected) { Sync-Entry $e } }
    "bump" {
        if ($Names.Count -lt 1) { throw "bump <name> [tag]" }
        $e = $entries | Where-Object { $_.Name -eq $Names[0] }
        if (-not $e) { throw "no module '$($Names[0])' in modules.lock" }
        $dir = Repo-Dir $e
        $tag = if ($Names.Count -ge 2) { $Names[1] } else { (& git -C $dir tag --list "v*" --sort=-v:refname | Select-Object -First 1) }
        if (-not $tag) { throw "$dir has no v* tag" }
        $commit = @(Git-Lines $dir @("rev-parse", "$tag^{commit}"))[0].Trim()
        # Every module of the same repo moves together (MealKit-Online: online and its backends)
        foreach ($other in $entries | Where-Object { $_.Url -eq $e.Url }) { $other.Tag = $tag; $other.Commit = $commit }
        Write-Lock $entries
        foreach ($other in $entries | Where-Object { $_.Url -eq $e.Url }) { Sync-Entry $other }
        Write-Host "Commit modules.lock and modules/ together in the engine (one engine commit per bump)."
    }
    "fetch" {
        if (-not $Project) { throw "fetch -Project <game project folder>" }
        $projectFile = Join-Path (Resolve-Path $Project).Path "mealkit.project"
        $cache = Join-Path $env:LOCALAPPDATA "MealKit\modules"
        $count = 0
        foreach ($line in [IO.File]::ReadAllLines($projectFile)) {
            if ($line.Trim() -notmatch '^engineModuleVersion\s+([a-z0-9-]+)\s+(\S+)') { continue }
            $name = $Matches[1]; $tag = $Matches[2]
            $e = $entries | Where-Object { $_.Name -eq $name }
            if (-not $e) { throw "${projectFile}: no engine module '$name' in modules.lock" }
            $target = Join-Path $cache "$name@$tag"
            if (Test-Path (Join-Path $target "engine-module.txt")) { Write-Host "have $name $tag"; $count++; continue }
            $dir = Repo-Dir $e
            $commit = @(Git-Lines $dir @("rev-parse", "$tag^{commit}"))[0].Trim()
            $pinned = [pscustomobject]@{ Name = $e.Name; Url = $e.Url; Tag = $tag; Commit = $commit; Path = $e.Path }
            New-Item -ItemType Directory -Force $target | Out-Null
            Sync-Entry $pinned $target
            $count++
        }
        Write-Host "$count pinned module version(s) ready in $cache"
    }
    "local" {
        if ($Names.Count -lt 1 -or $Names[0] -notin @("on", "off")) { throw "local on|off [name ...]" }
        $mode = $Names[0]
        $only = @($Names | Select-Object -Skip 1)
        foreach ($e in $entries) {
            if ($only.Count -and $only -notcontains $e.Name) { continue }
            $target = Vendored-Dir $e
            $sibling = Join-Path $workspace (Repo-Name $e.Url)
            if ($e.Path -ne ".") { $sibling = Join-Path $sibling $e.Path }
            if ($mode -eq "on") {
                if ((Is-Junction $target) -or -not (Test-Path $sibling)) { continue }
                $dirty = & git -C $engine status --porcelain -- "modules/$($e.Name)"
                if ($dirty) { Write-Host "skipped $($e.Name): modules\$($e.Name) has uncommitted changes"; continue }
                Remove-Item -Recurse -Force $target
                New-Item -ItemType Junction -Path $target -Target $sibling | Out-Null
                Write-Host "local $($e.Name) -> $sibling"
            } else {
                if (-not (Is-Junction $target)) { continue }
                (Get-Item $target).Delete() # removes the junction only, not the checkout's files
                & git -C $engine checkout -- "modules/$($e.Name)"
                Write-Host "vendored $($e.Name) restored"
            }
        }
    }
}

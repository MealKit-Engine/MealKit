# Reference samples fetched from their own repos into samples/<name> (docs/DECISIONS.md 2026-11-24 "The reference samples leave the engine repo").
# samples/<name> is git-ignored: samples.lock pins one commit per engine version, and this script fills the folders.
#   pwsh -NoProfile -File tools\sample.ps1 status                  folder vs samples.lock vs the local checkout (exit 1 when a sample is missing or differs)
#   pwsh -NoProfile -File tools\sample.ps1 sync [name ...]         rewrite samples/<name> from the locked commit
#   pwsh -NoProfile -File tools\sample.ps1 bump <name> [tag]       lock a new tag (default: the checkout's newest v* tag) and sync it
#   pwsh -NoProfile -File tools\sample.ps1 local on|off [name ...]
#       on:  samples/<name> becomes a junction (symbolic link on Linux/macOS) to the sibling checkout ..\sample-<name>, for changes that span the
#            engine and a sample. off: removes the link and syncs the locked copy again.
# Repos are read from sibling checkouts (D:\MealKit-Engine\sample-<name>), cloned from the lock's URL into %LOCALAPPDATA%\MealKit\samples when
# missing. A copy is the repo's files at the locked commit without .github/; build/, export/ and local notes (CLAUDE.md) are left alone.
param(
    [Parameter(Mandatory = $true, Position = 0)][ValidateSet("status", "sync", "bump", "local")][string]$Command,
    [Parameter(Position = 1, ValueFromRemainingArguments = $true)][string[]]$Names = @()
)
$ErrorActionPreference = "Stop"
$engine = Split-Path -Parent $PSScriptRoot
$lockPath = Join-Path $engine "samples.lock"
$workspace = Split-Path -Parent $engine
. "$PSScriptRoot\lock_repo.ps1"

$skip = { param($rel) $rel.StartsWith(".github/") }
# Output and notes that are not part of the sample repo
function Is-Local-File([string]$rel) { return $rel -eq "CLAUDE.md" -or $rel.StartsWith("build/") -or $rel.StartsWith("export/") }

function Read-Lock { return Read-LockFile $lockPath "sample" 5 }
function Write-Lock($entries) {
    Write-LockFile $lockPath @("# Reference samples fetched into samples/<name> (tools\sample.ps1). One engine version = VERSION + this file.",
                               "# sample <name> <repo url> <tag> <commit>",
                               "@format sample-lock 1") "sample" $entries $false
}

function Repo-Dir($e) { return Get-RepoDir $e $workspace "samples" }
function Sample-Dir($e) { return Join-Path $engine ("samples\" + $e.Name) }

# Differences between samples/<name> and the locked commit
function Sample-Differences($e, $dir) {
    $locked = Get-LockedFiles $e $dir $skip
    $target = Sample-Dir $e
    $diff = @()
    foreach ($rel in $locked.Keys) {
        $full = Join-Path $target $rel
        if (-not (Test-Path $full)) { $diff += "missing $rel"; continue }
        $blob = (& git hash-object --no-filters $full).Trim()
        if ($blob -ne $locked[$rel]) {
            $normalized = (& git -C $dir hash-object --path $rel $full).Trim() # line ending conversion
            if ($normalized -ne $locked[$rel]) { $diff += "changed $rel" }
        }
    }
    foreach ($f in Get-ChildItem -Path $target -Recurse -File -Force) {
        $rel = $f.FullName.Substring($target.Length + 1).Replace("\", "/")
        if ((Is-Local-File $rel) -or $locked.ContainsKey($rel)) { continue }
        $diff += "extra $rel"
    }
    return $diff
}

function Sync-Entry($e) {
    $dir = Repo-Dir $e
    $target = Sample-Dir $e
    if (Is-Junction $target) { throw "samples\$($e.Name) is in local mode; run 'sample.ps1 local off $($e.Name)' first" }
    $locked = Get-LockedFiles $e $dir $skip
    if (Test-Path $target) {
        foreach ($f in Get-ChildItem -Path $target -Recurse -File -Force) {
            $rel = $f.FullName.Substring($target.Length + 1).Replace("\", "/")
            if (-not (Is-Local-File $rel)) { Remove-Item -LiteralPath $f.FullName -Force }
        }
    }
    foreach ($rel in $locked.Keys) {
        $full = Join-Path $target $rel
        New-Item -ItemType Directory -Force (Split-Path -Parent $full) | Out-Null
        Copy-GitBlob $dir $locked[$rel] $full
    }
    Remove-EmptyDirectories $target
    Write-Host "synced $($e.Name) $($e.Tag) ($($e.Commit.Substring(0, 9))) -> $target"
}

$entries = Read-Lock
$selected = if ($Names.Count -and $Command -ne "bump" -and $Command -ne "local") { $entries | Where-Object { $Names -contains $_.Name } } else { $entries }
if ($Names.Count -and $Command -eq "sync" -and $selected.Count -ne $Names.Count) { throw "not in samples.lock: $((@($Names | Where-Object { $entries.Name -notcontains $_ })) -join ', ')" }

switch ($Command) {
    "status" {
        $bad = 0
        foreach ($e in $selected) {
            $target = Sample-Dir $e
            if (Is-Junction $target) { Write-Host ("{0,-14} LOCAL  -> {1}" -f $e.Name, (Get-Item $target).Target); continue }
            if (-not (Test-Path $target)) { $bad++; Write-Host ("{0,-14} MISSING {1}  (tools\sample.ps1 sync)" -f $e.Name, $e.Tag) -ForegroundColor Red; continue }
            $dir = Repo-Dir $e
            $diff = Sample-Differences $e $dir
            $head = @(Git-Lines $dir @("rev-parse", "HEAD"))[0].Trim()
            $note = if ($head -ne $e.Commit) { "checkout at $($head.Substring(0, 9)), newest tag $(Newest-Tag $dir)" } else { "" }
            if ($diff.Count) { $bad++; Write-Host ("{0,-14} DIFF   {1} {2}" -f $e.Name, $e.Tag, ($diff -join ", ")) -ForegroundColor Red }
            else { Write-Host ("{0,-14} ok     {1} {2}" -f $e.Name, $e.Tag, $note) }
        }
        if ($bad) { exit 1 }
    }
    "sync" { foreach ($e in $selected) { Sync-Entry $e } }
    "bump" {
        if ($Names.Count -lt 1) { throw "bump <name> [tag]" }
        $e = $entries | Where-Object { $_.Name -eq $Names[0] }
        if (-not $e) { throw "no sample '$($Names[0])' in samples.lock" }
        $dir = Repo-Dir $e
        $tag = if ($Names.Count -ge 2) { $Names[1] } else { Newest-Tag $dir }
        if (-not $tag) { throw "$dir has no v* tag" }
        $commit = @(Git-Lines $dir @("rev-parse", "$tag^{commit}"))[0].Trim()
        $e.Tag = $tag
        $e.Commit = $commit
        Write-Lock $entries
        if (Is-Junction (Sample-Dir $e)) { Write-Host "samples\$($e.Name) is in local mode: locked $tag, run 'sample.ps1 local off $($e.Name)' to sync it" }
        else { Sync-Entry $e }
        Write-Host "Commit samples.lock in the engine (one engine commit per bump)."
    }
    "local" {
        if ($Names.Count -lt 1 -or $Names[0] -notin @("on", "off")) { throw "local on|off [name ...]" }
        $mode = $Names[0]
        $only = @($Names | Select-Object -Skip 1)
        foreach ($e in $entries) {
            if ($only.Count -and $only -notcontains $e.Name) { continue }
            $target = Sample-Dir $e
            $sibling = Join-Path $workspace (Repo-Name $e.Url)
            if ($mode -eq "on") {
                if ((Is-Junction $target) -or -not (Test-Path $sibling)) { continue }
                if (Test-Path $target) {
                    $diff = Sample-Differences $e (Repo-Dir $e)
                    if ($diff.Count) { Write-Host "skipped $($e.Name): samples\$($e.Name) has changes not in the lock ($($diff.Count) files)"; continue }
                    Remove-Item -Recurse -Force $target
                }
                New-DirectoryLink $target $sibling
                Write-Host "local $($e.Name) -> $sibling"
            } else {
                if (-not (Is-Junction $target)) { continue }
                (Get-Item $target).Delete() # removes the link only, not the checkout's files
                Sync-Entry $e
            }
        }
    }
}

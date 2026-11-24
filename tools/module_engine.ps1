# Engine modules vendored from their own repos into the engine (docs/ENGINE_MODULES.md "Lock file and vendored copies"). A game adds,
# updates and removes engine modules with tools\module.ps1, like source modules; this script only keeps the engine's copies.
# Called as tools\module.ps1 engine <command> (internal; tools\modules.ps1 forwards here for old command lines).
#   powershell -ExecutionPolicy Bypass -File tools\module.ps1 engine status            vendored copy vs modules.lock vs the local checkout
#   powershell -ExecutionPolicy Bypass -File tools\module.ps1 engine sync [name ...]   rewrite modules/<name> from the locked commit
#   powershell -ExecutionPolicy Bypass -File tools\module.ps1 engine bump <name> [tag] lock a new tag (default: the checkout's newest v* tag) and sync it
#   powershell -ExecutionPolicy Bypass -File tools\module.ps1 engine local on|off [name ...]
#       on:  modules/<name> becomes a junction to the sibling checkout ..\module-<name>, for changes that span repos (the engine build
#            uses the checkout's working files). off: removes the junctions and restores the vendored copies from git.
# Repos are read from sibling checkouts (D:\MealKit-Engine\module-<name>), cloned from the lock's URL into
# %LOCALAPPDATA%\MealKit\modules when missing. A vendored copy is the repo's files at the locked commit without README.md and .github/;
# local notes (CLAUDE.md) are left alone; local on keeps them under .git\mealkit-local-notes until local off.
param(
    [Parameter(Mandatory = $true, Position = 0)][ValidateSet("status", "sync", "bump", "local")][string]$Command,
    [Parameter(Position = 1, ValueFromRemainingArguments = $true)][string[]]$Names = @()
)
$ErrorActionPreference = "Stop"
$engine = Split-Path -Parent $PSScriptRoot
$lockPath = Join-Path $engine "modules.lock"
$workspace = Split-Path -Parent $engine
. "$PSScriptRoot\lock_repo.ps1"
. "$PSScriptRoot\vendor_header.ps1"
$moduleSkip = { param($rel) $rel -eq "README.md" -or $rel.StartsWith(".github/") -or $rel.StartsWith("backends/") }

# ---- modules.lock: "module <name> <url> <tag> <commit> <path in repo>"
function Read-Lock { return Read-LockFile $lockPath "module" 6 }

function Write-Lock($entries) {
    Write-LockFile $lockPath @("# Engine modules vendored into modules/<name> (tools\module.ps1 engine). One engine version = VERSION + this file.",
                               "# module <name> <repo url> <tag> <commit> <path in the repo>",
                               "@format module-lock 1") "module" $entries $true
}

function Repo-Dir($e) { return Get-RepoDir $e $workspace "modules" }

# Files of the vendored copy at the locked commit: relative path -> blob id
function Locked-Files($e, $dir) { return Get-LockedFiles $e $dir $moduleSkip }

function Vendored-Dir($e) { return Join-Path $engine ("modules\" + $e.Name) }

function Vendored-Differences($e, $dir) {
    $locked = Locked-Files $e $dir
    $target = Vendored-Dir $e
    $diff = @()
    foreach ($rel in $locked.Keys) {
        $full = Join-Path $target $rel
        if (-not (Test-Path $full)) { $diff += "missing $rel"; continue }
        $stripped = New-VendorStrippedCopy $full # compare without the vendored-copy header line
        $hashFile = if ($stripped) { $stripped } else { $full }
        try {
        $blob = (& git hash-object --no-filters $hashFile).Trim()
        if ($blob -ne $locked[$rel]) {
            $normalized = (& git -C $engine hash-object --path ("modules/$($e.Name)/" + $rel) $hashFile).Trim() # line ending conversion
            if ($normalized -ne $locked[$rel]) { $diff += "changed $rel" }
        }
        } finally { if ($hashFile -ne $full) { Remove-Item -LiteralPath $hashFile -Force -ErrorAction SilentlyContinue } }
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
    if (-not $into -and (Is-Junction $target)) { throw "modules\$($e.Name) is in local mode; run 'module.ps1 engine local off $($e.Name)' first" }
    $locked = Locked-Files $e $dir
    if (Test-Path $target) {
        foreach ($f in Get-ChildItem -Path $target -Recurse -File) {
            if ($f.Name -ne "CLAUDE.md") { Remove-Item -LiteralPath $f.FullName -Force }
        }
    }
    foreach ($rel in $locked.Keys) {
        $full = Join-Path $target $rel
        New-Item -ItemType Directory -Force (Split-Path -Parent $full) | Out-Null
        Copy-GitBlob $dir $locked[$rel] $full
        Set-VendorHeader $full $e.Name $e.Tag # "vendored copy of module-<name>" first line of C/C++ files (tools\vendor_header.ps1)
    }
    # Empty folders left by removed files
    Remove-EmptyDirectories $target
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
            $newest = (Newest-Tag $dir)
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
        $tag = if ($Names.Count -ge 2) { $Names[1] } else { (Newest-Tag $dir) }
        if (-not $tag) { throw "$dir has no v* tag" }
        $commit = @(Git-Lines $dir @("rev-parse", "$tag^{commit}"))[0].Trim()
        # Every module of the same repo moves together (module-online: online and its backends)
        foreach ($other in $entries | Where-Object { $_.Url -eq $e.Url }) { $other.Tag = $tag; $other.Commit = $commit }
        Write-Lock $entries
        foreach ($other in $entries | Where-Object { $_.Url -eq $e.Url }) { Sync-Entry $other }
        Write-Host "Commit modules.lock and modules/ together in the engine (one engine commit per bump)."
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
                # Local notes (untracked CLAUDE.md files) would go with the folder: kept under .git until local off
                $notes = Join-Path $engine ".git\mealkit-local-notes\$($e.Name)"
                Get-ChildItem -LiteralPath $target -Recurse -File -Filter "CLAUDE.md" | ForEach-Object {
                    $rel = $_.FullName.Substring($target.Length + 1)
                    $dest = Join-Path $notes $rel
                    New-Item -ItemType Directory -Force -Path (Split-Path $dest) | Out-Null
                    Copy-Item -LiteralPath $_.FullName -Destination $dest -Force
                }
                Remove-Item -Recurse -Force $target
                try { New-DirectoryLink $target $sibling } catch { throw "$_ (run 'git checkout -- modules/$($e.Name)' to restore it)" }
                Write-Host "local $($e.Name) -> $sibling"
            } else {
                if (-not (Is-Junction $target)) { continue }
                (Get-Item $target).Delete() # removes the junction only, not the checkout's files
                & git -C $engine checkout -- "modules/$($e.Name)"
                $notes = Join-Path $engine ".git\mealkit-local-notes\$($e.Name)"
                if (Test-Path $notes) {
                    Get-ChildItem -LiteralPath $notes -Recurse -File | ForEach-Object {
                        $dest = Join-Path $target $_.FullName.Substring($notes.Length + 1)
                        New-Item -ItemType Directory -Force -Path (Split-Path $dest) | Out-Null
                        Copy-Item -LiteralPath $_.FullName -Destination $dest -Force
                    }
                    Remove-Item -Recurse -Force $notes
                }
                Write-Host "vendored $($e.Name) restored"
            }
        }
    }
}

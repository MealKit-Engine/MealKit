# Adds, updates, and removes source modules in the project's modules/<name>/ (docs/MODULES.md). The editor and MealKit Manager call this too.
#   powershell -ExecutionPolicy Bypass -File tools\module.ps1 search [<query>] [-Tag <tag>] [-Project <folder>] [-Tsv]   (-Tsv: lines for the editor/Manager)
#   powershell -ExecutionPolicy Bypass -File tools\module.ps1 list   -Project <folder>
#   powershell -ExecutionPolicy Bypass -File tools\module.ps1 add    -Project <folder> <name[@version] | folder | git url[#tag]> [-Name <module>] [-WithRequires]
#   powershell -ExecutionPolicy Bypass -File tools\module.ps1 update -Project <folder> <module> [-To <version>] [-From <folder | git url[#tag]>] [-DryRun] [-Force]
#   powershell -ExecutionPolicy Bypass -File tools\module.ps1 remove -Project <folder> <module> [-Force]
#
# - The source is a module folder (module.txt at its root) or a git repo containing such folders. If the repo has several modules/<name>/module.txt, pick one with -Name.
#   Names are looked up in the registry (MealKit-Modules) index.json: fetches the newest version for this engine MINOR by tag and checks the commit matches the list.
#   The list can be changed with -Index or the environment variable MEALKIT_MODULE_INDEX (URL or file). The fetched list is kept in %LOCALAPPDATA%\MealKit\module-index.json and used when offline.
# - What's copied: module.txt, sim/, view/, assets/, art/, docs/, LICENSE*/README* at the root (excluding *.new). Hashes of copied files and where they came from are kept in modules/<name>/module.files.
# - update follows the same rules as upgrade_project.ps1: only unmodified files (hash matches the record) are replaced; modified files are kept and <file>.new is written.
#   Files gone in the new version are deleted if unmodified, kept and reported if modified. -Force replaces modified files too.
# - remove refuses if another module's requires lists it or game source/ includes it (force with -Force). Undo is git's job.
param(
    [Parameter(Mandatory = $true, Position = 0)][ValidateSet("search", "list", "add", "update", "remove")][string]$Command,
    [Parameter(Position = 1)][string]$Target = "",
    [string]$Project = "",
    [string]$Name = "",
    [string]$From = "",
    [string]$To = "",
    [string]$Tag = "",
    [string]$Index = "",
    [switch]$WithRequires,
    [switch]$Tsv,
    [switch]$DryRun,
    [switch]$Force
)
$ErrorActionPreference = "Stop"
. "$PSScriptRoot\project_common.ps1"

$modulesDir = ""
if ($Command -ne "search" -or $Project) { # search works without a project too (shows installed state if given)
    if ($Project -eq "") { $Project = (Get-Location).Path }
    $Project = (Get-Item -LiteralPath (Resolve-Path $Project).Path -Force).FullName.TrimEnd("\") # long path (not an 8.3 short name)
    if (-not (Test-Path (Join-Path $Project "mealkit.project"))) { Write-Error "$Project has no mealkit.project"; exit 1 }
    $modulesDir = Join-Path $Project "modules"
}
$filesName = "module.files"
# Only the module structure is copied (docs/MODULES.md): .git, .github, tests/, build folders etc. don't go into the project
$copyTop = @("module.txt", "sim", "view", "assets", "art", "docs")
$textExtensions = @(".h", ".hpp", ".cpp", ".c", ".inl", ".txt", ".md", ".ui", ".table", ".tuning", ".scene", ".events", ".bindings",
    ".json", ".yml", ".yaml", ".gltf", ".cube", ".fx", ".gitignore", ".gitattributes")
$engineMinor = ((Get-MealKitVersion) -replace '^(\d+\.\d+).*$', '$1')
$defaultIndex = "https://raw.githubusercontent.com/MealKit-Engine/MealKit-Modules/main/index.json"
$registryNamePattern = '^[a-z][a-z0-9_]*(@\d+\.\d+\.\d+)?$'

# ---------------------------------------------------------------- Common

function Read-ModuleInfo([string]$dir) {
    $info = @{ name = ""; version = "0"; engine = ""; requires = @() }
    foreach ($line in [IO.File]::ReadAllLines((Join-Path $dir "module.txt"), $MealKitUtf8)) {
        if ($line -match '^\s*(name|version|engine|requires)(?:\s+(.*?))?\s*$') {
            $value = "$($Matches[2])"
            if ($Matches[1] -eq "requires") {
                $info.requires = @($value -split '\s+' | Where-Object { $_ -ne "" -and $_ -notmatch '^[0-9]' })
            } else { $info[$Matches[1]] = $value }
        }
    }
    return $info
}

# Hash independent of line endings (text), byte hash (everything else)
function Get-ModuleFileHash([string]$file) {
    if ($textExtensions -contains [IO.Path]::GetExtension($file).ToLowerInvariant()) {
        return Get-TextHash ([IO.File]::ReadAllText($file, $MealKitUtf8))
    }
    $sha = [Security.Cryptography.SHA256]::Create()
    return ([BitConverter]::ToString($sha.ComputeHash([IO.File]::ReadAllBytes($file)))).Replace("-", "").ToLowerInvariant()
}

# Files to copy from a module folder: module-relative path (/) → full path
function Get-ModuleFiles([string]$dir) {
    $files = [ordered]@{}
    # The base is the long path from Get-Item (paths received as 8.3 short names like %TEMP% differ in length from FullName)
    $base = (Get-Item -LiteralPath $dir -Force).FullName.TrimEnd("\")
    Get-ChildItem -LiteralPath $base -Recurse -File -Force | Sort-Object FullName | ForEach-Object {
        $rel = $_.FullName.Substring($base.Length + 1).Replace("\", "/")
        $top = $rel.Split("/")[0]
        $keep = $copyTop -contains $top -or ($rel -notmatch "/" -and $top -match "^(license|readme)")
        if (-not $keep -or $_.Extension -eq ".new") { return }
        $files[$rel] = $_.FullName
    }
    return $files
}

function Read-FilesRecord([string]$dir) {
    $record = @{ source = ""; hashes = @{} }
    $file = Join-Path $dir $filesName
    if (Test-Path $file) {
        foreach ($line in [IO.File]::ReadAllLines($file, $MealKitUtf8)) {
            if ($line -match '^source\s+(.+)$') { $record.source = $Matches[1] }
            elseif ($line -match '^([0-9a-f]{64})\s+(.+)$') { $record.hashes[$Matches[2]] = $Matches[1] }
        }
    }
    return $record
}

function Write-FilesRecord([string]$dir, [string]$source, [hashtable]$hashes) {
    $lines = @("# MealKit module files (sha256  path). tools\module.ps1 update replaces a file only while its hash matches.", "source $source")
    foreach ($k in ($hashes.Keys | Sort-Object)) { $lines += "$($hashes[$k])  $k" }
    Write-TextFile (Join-Path $dir $filesName) (($lines -join "`r`n") + "`r`n")
}

# Registry list (MealKit-Modules index.json). If it can't be fetched, the last fetched cache
$script:registry = $null
function Get-Registry {
    if ($script:registry) { return $script:registry }
    $spec = if ($Index) { $Index } elseif ($env:MEALKIT_MODULE_INDEX) { $env:MEALKIT_MODULE_INDEX } else { $defaultIndex }
    $cache = Join-Path ([Environment]::GetFolderPath("LocalApplicationData")) "MealKit\module-index.json"
    if (Test-Path -LiteralPath $spec) {
        $text = [IO.File]::ReadAllText((Resolve-Path -LiteralPath $spec).Path, $MealKitUtf8)
    } else {
        try {
            [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
            $client = New-Object Net.WebClient
            $client.Encoding = [Text.Encoding]::UTF8
            $text = $client.DownloadString($spec)
            New-Item -ItemType Directory -Force (Split-Path -Parent $cache) | Out-Null
            [IO.File]::WriteAllText($cache, $text, $MealKitUtf8)
        } catch {
            if (-not (Test-Path $cache)) { throw "cannot read the module list $spec ($($_.Exception.Message)) and there is no cached copy" }
            Write-Host "warning: cannot read $spec - using the list cached $((Get-Item $cache).LastWriteTime)" -ForegroundColor Yellow
            $text = [IO.File]::ReadAllText($cache, $MealKitUtf8)
        }
    }
    $script:registry = $text | ConvertFrom-Json
    return $script:registry
}

function Find-RegistryModule([string]$name) {
    $m = @((Get-Registry).modules | Where-Object { $_.name -eq $name })
    if ($m.Count -eq 0) { throw "no module '$name' in the registry (tools\module.ps1 search)" }
    return $m[0]
}

# Version to pick: the given version, otherwise the newest for this engine MINOR, failing that the newest version (warning)
function Select-RegistryVersion($module, [string]$version) {
    $versions = @($module.versions) # build_index.py writes newest first
    if ($version) {
        $v = @($versions | Where-Object { $_.version -eq $version })
        if ($v.Count -eq 0) { throw "$($module.name) has no version $version (has $(($versions | ForEach-Object version) -join ', '))" }
        return $v[0]
    }
    $fit = @($versions | Where-Object { $_.engine -eq $engineMinor })
    if ($fit.Count -gt 0) { return $fit[0] }
    Write-Host "warning: no $($module.name) version is made for engine $engineMinor - taking $($versions[0].version) (engine $($versions[0].engine))" -ForegroundColor Yellow
    return $versions[0]
}

# Source (registry name[@version], folder, git url#tag) → @{ dir; source } . git is freshly cloned into a cache folder
function Get-ModuleSource([string]$spec, [string]$want) {
    $root = ""
    $source = ""
    if (-not (Test-Path $spec) -and $spec -match $registryNamePattern) {
        $rname, $rver = $spec.Split("@", 2)
        $module = Find-RegistryModule $rname
        $v = Select-RegistryVersion $module $rver
        $src = Get-ModuleSource "$($module.repo)#$($v.tag)" ""
        $commit = ($src.source -split '\s+')[3]
        if ($commit -ne $v.commit) {
            Remove-Cache $src
            throw "$($module.repo) tag $($v.tag) is now $commit, but the registry lists $($v.commit) - the tag was moved, not adding it"
        }
        $src.source = "registry $rname $($v.version) $commit $($module.repo) $($v.tag)"
        return $src
    }
    if (Test-Path $spec) {
        $root = (Resolve-Path $spec).Path.TrimEnd("\")
        $source = "folder $root"
    } elseif ($spec -match '^(https?://|git@|ssh://|file://)' -or $spec -match '\.git(#.*)?$') {
        $url, $ref = $spec.Split("#", 2)
        $cacheRoot = Join-Path ([Environment]::GetFolderPath("LocalApplicationData")) "MealKit\module-cache"
        $root = Join-Path $cacheRoot ([Guid]::NewGuid().ToString("N").Substring(0, 12))
        New-Item -ItemType Directory -Force $cacheRoot | Out-Null
        $gitArgs = @("-c", "advice.detachedHead=false", "clone", "--quiet", "--depth", "1")
        if ($ref) { $gitArgs += @("--branch", $ref) }
        # git writes notices to stderr too: with Stop, PowerShell 5.1 would abort on them as errors, so only the exit code decides
        $ErrorActionPreference = "Continue"
        & git @gitArgs $url $root 2>&1 | ForEach-Object { Write-Host "  git: $_" }
        $cloned = $LASTEXITCODE -eq 0
        $ErrorActionPreference = "Stop"
        if (-not $cloned) {
            Remove-Item -Recurse -Force $root -ErrorAction SilentlyContinue
            throw "git clone $spec failed"
        }
        $commit = (& git -C $root rev-parse HEAD).Trim()
        $source = "git $url $(if ($ref) { $ref } else { '-' }) $commit"
    } else {
        throw "'$spec' is not a registry module name, a folder or a git address"
    }
    if (Test-Path (Join-Path $root "module.txt")) { return @{ dir = $root; source = $source } }
    $found = @(Get-ChildItem (Join-Path $root "modules") -Directory -ErrorAction SilentlyContinue |
        Where-Object { Test-Path (Join-Path $_.FullName "module.txt") })
    if ($want) { $found = @($found | Where-Object { $_.Name -eq $want }) }
    if ($found.Count -eq 1) { return @{ dir = $found[0].FullName; source = "$source modules/$($found[0].Name)" } }
    if ($found.Count -eq 0) { throw "no module.txt in $spec$(if ($want) { " for '$want'" })" }
    throw "$spec has several modules ($(($found | ForEach-Object Name) -join ', ')) - pick one with -Name"
}

function Remove-Cache([hashtable]$src) {
    $cacheRoot = Join-Path ([Environment]::GetFolderPath("LocalApplicationData")) "MealKit\module-cache"
    if ($src.dir.StartsWith($cacheRoot, [StringComparison]::OrdinalIgnoreCase)) {
        $top = $src.dir.Substring($cacheRoot.Length + 1).Split("\")[0]
        Remove-Item -Recurse -Force (Join-Path $cacheRoot $top) -ErrorAction SilentlyContinue
    }
}

function Test-ModuleWarnings([hashtable]$info) {
    if ($info.engine -and $info.engine -ne $engineMinor) {
        Write-Host "warning: $($info.name) $($info.version) is made for engine $($info.engine), this engine is $(Get-MealKitVersion)" -ForegroundColor Yellow
    }
    foreach ($r in $info.requires) {
        if (-not (Test-Path (Join-Path $modulesDir "$r\module.txt"))) {
            Write-Host "warning: $($info.name) needs the module '$r' - add it too (the build stops without it)" -ForegroundColor Yellow
        }
    }
}

# One installed module: module.txt, number of files differing from the record (modified), remaining .new count, source
function Get-ModuleState([string]$dir) {
    $record = Read-FilesRecord $dir
    $modified = 0
    foreach ($rel in $record.hashes.Keys) {
        $file = Join-Path $dir $rel
        if (-not (Test-Path $file) -or (Get-ModuleFileHash $file) -ne $record.hashes[$rel]) { $modified++ }
    }
    return @{ info = (Read-ModuleInfo $dir); modified = $modified; pending = @(Get-ChildItem $dir -Recurse -File -Filter *.new).Count
        source = $(if ($record.source) { $record.source } else { "(no module.files)" }) }
}

function Get-Installed {
    if (-not (Test-Path $modulesDir)) { return @() }
    return @(Get-ChildItem $modulesDir -Directory | Where-Object { Test-Path (Join-Path $_.FullName "module.txt") } | Sort-Object Name)
}

# ---------------------------------------------------------------- Commands

switch ($Command) {
    "search" {
        if ($Tsv) {
            # Lines read by the editor [Modules] window and the MealKit Manager [Modules] tab (tab-separated, UTF-8). One call returns both installed modules and the list:
            #   installed <name> <version> <modified file count> <.new count> <source>
            #   module <name> <version for this engine or -> <newest version> <engine of that version> <title> <summary> <tags (space)> <author> <repo>
            #   error <message>   (the list couldn't be fetched. installed lines still appear)
            [Console]::OutputEncoding = New-Object Text.UTF8Encoding $false
            $clean = { param($s) "$s" -replace "[`t`r`n]", " " }
            foreach ($m in Get-Installed) {
                $st = Get-ModuleState $m.FullName
                Write-Output (@("installed", $st.info.name, $st.info.version, $st.modified, $st.pending, (& $clean $st.source)) -join "`t")
            }
            try { $all = @((Get-Registry).modules) } catch { Write-Output "error`t$(& $clean $_.Exception.Message)"; $all = @() }
            foreach ($m in $all) {
                $fit = @($m.versions | Where-Object { $_.engine -eq $engineMinor })
                $first = @($m.versions)[0]
                Write-Output (@("module", $m.name, $(if ($fit.Count) { $fit[0].version } else { "-" }), $first.version, $first.engine,
                    (& $clean $m.title), (& $clean $m.summary), ($m.tags -join " "), (& $clean $m.author), $m.repo) -join "`t")
            }
            break
        }
        $words = @("$Target".ToLowerInvariant() -split '\s+' | Where-Object { $_ })
        $hits = @((Get-Registry).modules | Where-Object {
            $m = $_
            $text = "$($m.name) $($m.title) $($m.summary) $($m.tags -join ' ') $($m.author)".ToLowerInvariant()
            (-not $Tag -or $m.tags -contains $Tag) -and -not ($words | Where-Object { -not $text.Contains($_) })
        })
        if ($hits.Count -eq 0) { Write-Host "No modules found"; break }
        # Those with a version for this engine MINOR first
        $hits = $hits | Sort-Object @{ Expression = { @($_.versions | Where-Object { $_.engine -eq $engineMinor }).Count -eq 0 } }, name
        foreach ($m in $hits) {
            $fit = @($m.versions | Where-Object { $_.engine -eq $engineMinor })
            $ver = if ($fit.Count) { $fit[0].version } else { "$($m.latest) (engine $(@($m.versions)[0].engine), not $engineMinor)" }
            $mark = if ($modulesDir -and (Test-Path (Join-Path $modulesDir "$($m.name)\module.txt"))) { "  [installed]" } else { "" }
            Write-Host ("{0} {1}  {2}{3}" -f $m.name, $ver, $m.title, $mark) -ForegroundColor $(if ($fit.Count) { "White" } else { "DarkGray" })
            Write-Host "    $($m.summary)"
            Write-Host "    by $($m.author)  tags: $($m.tags -join ', ')  $($m.repo)" -ForegroundColor DarkGray
        }
    }

    "list" {
        $installed = Get-Installed
        if ($installed.Count -eq 0) { Write-Host "No modules in $modulesDir"; break }
        foreach ($m in $installed) {
            $st = Get-ModuleState $m.FullName
            Write-Host ("{0} {1}  modified {2}, .new {3}  from {4}" -f $st.info.name, $st.info.version, $st.modified, $st.pending, $st.source)
        }
    }

    "add" {
        if (-not $Target) { Write-Error "add needs a module name, a folder or a git address"; exit 1 }
        $src = Get-ModuleSource $Target $Name
        try {
            $info = Read-ModuleInfo $src.dir
            if ($info.name -notmatch '^[a-z0-9_]+$') { throw "module name '$($info.name)' must be lowercase letters, digits and _" }
            $dest = Join-Path $modulesDir $info.name
            if (Test-Path $dest) { throw "modules/$($info.name) already exists - use 'update'" }
            $hashes = @{}
            foreach ($entry in (Get-ModuleFiles $src.dir).GetEnumerator()) {
                $to = Join-Path $dest $entry.Key.Replace("/", "\")
                New-Item -ItemType Directory -Force (Split-Path -Parent $to) | Out-Null
                Copy-Item -LiteralPath $entry.Value -Destination $to -Force
                $hashes[$entry.Key] = Get-ModuleFileHash $to
            }
            Write-FilesRecord $dest $src.source $hashes
            Write-Host "Added $($info.name) $($info.version) ($($hashes.Count) files) to modules/$($info.name)" -ForegroundColor Green
            if ($src.source.StartsWith("registry ")) {
                Write-Host "Note: module code runs inside the editor and the game. The registry checks that it builds, not what it does - read what you add." -ForegroundColor Yellow
            }
            $missing = @($info.requires | Where-Object { -not (Test-Path (Join-Path $modulesDir "$_\module.txt")) })
            if ($WithRequires) {
                foreach ($req in $missing) {
                    Write-Host "Adding required module $req"
                    & $PSCommandPath add $req -Project $Project -Index $Index -WithRequires
                    if ($LASTEXITCODE -ne 0) { throw "could not add the required module $req" }
                }
            } elseif ($missing.Count) {
                Write-Host "Also add: $($missing -join ', ') (or add again with -WithRequires)" -ForegroundColor Yellow
            }
            Test-ModuleWarnings $info
            $docs = Join-Path $dest "docs\index.md"
            if (Test-Path $docs) { Write-Host "Next: connect it to the game as its docs say ('Adding' section): $docs" }
        } finally { Remove-Cache $src }
    }

    "update" {
        if (-not $Target) { Write-Error "update needs a module name"; exit 1 }
        $dest = Join-Path $modulesDir $Target
        if (-not (Test-Path (Join-Path $dest "module.txt"))) { Write-Error "modules/$Target is not installed"; exit 1 }
        $record = Read-FilesRecord $dest
        $spec = $From
        if (-not $spec) {
            if ($record.source -match '^folder\s+(.+)$') { $spec = $Matches[1] }
            elseif ($record.source -match '^registry\s+(\S+)') { $spec = if ($To) { "$($Matches[1])@$To" } else { $Matches[1] } }
            elseif ($record.source -match '^git\s+(\S+)\s+(\S+)') { $spec = if ($Matches[2] -eq "-") { $Matches[1] } else { "$($Matches[1])#$($Matches[2])" } }
            else { Write-Error "modules/$Target has no recorded source - give one with -From"; exit 1 }
        }
        $src = Get-ModuleSource $spec $Target
        try {
            $old = Read-ModuleInfo $dest
            $info = Read-ModuleInfo $src.dir
            if ($info.name -ne $Target) { throw "$spec is the module '$($info.name)', not '$Target'" }
            Write-Host "Updating $Target $($old.version) -> $($info.version)$(if ($DryRun) { '  [dry run]' })"
            $newFiles = Get-ModuleFiles $src.dir
            $hashes = @{}
            $changed = 0
            $kept = 0
            foreach ($entry in $newFiles.GetEnumerator()) {
                $rel = $entry.Key
                $dst = Join-Path $dest $rel.Replace("/", "\")
                $newHash = Get-ModuleFileHash $entry.Value
                if (-not (Test-Path $dst)) {
                    Write-Host "  added     $rel" -ForegroundColor Green
                    if (-not $DryRun) { New-Item -ItemType Directory -Force (Split-Path -Parent $dst) | Out-Null; Copy-Item -LiteralPath $entry.Value $dst -Force }
                    $hashes[$rel] = $newHash; $changed++
                    continue
                }
                $currentHash = Get-ModuleFileHash $dst
                $recorded = $record.hashes[$rel]
                if ($currentHash -eq $newHash) {
                    $hashes[$rel] = $newHash
                } elseif ($Force -or $currentHash -eq $recorded) {
                    Write-Host "  updated   $rel" -ForegroundColor Green
                    if (-not $DryRun) { Copy-Item -LiteralPath $entry.Value $dst -Force }
                    $hashes[$rel] = $newHash; $changed++
                } elseif ($recorded -eq $newHash) {
                    $hashes[$rel] = $recorded # the module is unchanged and only the game modified it
                } else {
                    Write-Host "  modified  $rel  (kept; new version in $rel.new. -Force to replace)" -ForegroundColor Yellow
                    if (-not $DryRun) { Copy-Item -LiteralPath $entry.Value "$dst.new" -Force }
                    if ($recorded) { $hashes[$rel] = $recorded }
                    $kept++
                }
            }
            foreach ($rel in $record.hashes.Keys) {
                if ($newFiles.Contains($rel)) { continue }
                $dst = Join-Path $dest $rel.Replace("/", "\")
                if (-not (Test-Path $dst)) { continue }
                if ($Force -or (Get-ModuleFileHash $dst) -eq $record.hashes[$rel]) {
                    Write-Host "  removed   $rel" -ForegroundColor Green
                    if (-not $DryRun) { Remove-Item -LiteralPath $dst -Force }
                    $changed++
                } else {
                    Write-Host "  orphaned  $rel  (no longer in the module, but you changed it - kept)" -ForegroundColor Yellow
                    $kept++
                }
            }
            if (-not $DryRun) { Write-FilesRecord $dest $src.source $hashes }
            Write-Host "Done: $changed change(s), $kept kept"
            Test-ModuleWarnings $info
        } finally { Remove-Cache $src }
    }

    "remove" {
        if (-not $Target) { Write-Error "remove needs a module name"; exit 1 }
        $dest = Join-Path $modulesDir $Target
        if (-not (Test-Path $dest)) { Write-Error "modules/$Target is not installed"; exit 1 }
        $blockers = @()
        foreach ($m in Get-Installed) {
            if ($m.Name -ne $Target -and (Read-ModuleInfo $m.FullName).requires -contains $Target) { $blockers += "module $($m.Name) requires it" }
        }
        $sourceDir = Join-Path $Project "source"
        if (Test-Path $sourceDir) {
            $pattern = '#\s*include\s*["<]' + [regex]::Escape($Target) + '/'
            Get-ChildItem $sourceDir -Recurse -File -Include *.h, *.hpp, *.cpp | Select-String -Pattern $pattern | ForEach-Object {
                $blockers += "$($_.Path.Substring($Project.Length + 1)):$($_.LineNumber): $($_.Line.Trim())"
            }
        }
        if ($blockers.Count -gt 0 -and -not $Force) {
            $blockers | ForEach-Object { Write-Host "  $_" -ForegroundColor Yellow }
            Write-Error "modules/$Target is still used (above). Remove those uses first, or -Force"
            exit 1
        }
        Remove-Item -Recurse -Force $dest
        Write-Host "Removed modules/$Target" -ForegroundColor Green
    }
}
exit 0

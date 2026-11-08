# Adds, updates, and removes modules in the project's modules/<name>/: source modules (docs/MODULES.md) and engine modules (module.txt "type engine",
# docs/ENGINE_MODULES.md "Module selection"), the same way. The editor and MealKit Manager call this too.
#   powershell -ExecutionPolicy Bypass -File tools\module.ps1 search [<query>] [-Tag <tag>] [-Project <folder>] [-Tsv]   (-Tsv: lines for the editor/Manager)
#   powershell -ExecutionPolicy Bypass -File tools\module.ps1 list   -Project <folder>
#   powershell -ExecutionPolicy Bypass -File tools\module.ps1 add    -Project <folder> <name[@version] | folder | git url[#tag]> [-Name <module>] [-WithRequires]
#   powershell -ExecutionPolicy Bypass -File tools\module.ps1 add    -Project <folder> -Preset standard|solo    (every engine module of that preset)
#   powershell -ExecutionPolicy Bypass -File tools\module.ps1 update -Project <folder> <module> [-To <version>] [-From <folder | git url[#tag]>] [-DryRun] [-Force]
#   powershell -ExecutionPolicy Bypass -File tools\module.ps1 remove -Project <folder> <module> [-Force]
#
# - The source is a module folder (module.txt at its root) or a git repo containing such folders. If the repo has several modules/<name>/module.txt, pick one with -Name.
#   A name of an engine module (this engine's modules/<name>, e.g. render, audio, online-steam) takes the engine's copy; name@version takes that tag
#   (v<version>) of the module's repo in the engine's modules.lock (the checkout beside the engine when there is one).
#   A project with no engine modules in modules/ builds with every engine module of its engine; once it has one, only those in modules/.
#   Names are looked up in the registry (MealKit-Modules) index.json: fetches the newest version for this engine MINOR by tag and checks the commit matches the list.
#   The list can be changed with -Index or the environment variable MEALKIT_MODULE_INDEX (URL or file). The fetched list is kept in %LOCALAPPDATA%\MealKit\module-index.json and used when offline.
# - What's copied: module.txt, sim/, view/, assets/, art/, docs/, LICENSE*/README* at the root (excluding *.new); for an engine module every file but
#   .git/, .github/, backends/ (separate modules), CLAUDE.md and README*. Hashes of copied files and where they came from are kept in modules/<name>/module.files.
# - update follows the same rules as upgrade_project.ps1: only unmodified files (hash matches the record) are replaced; modified files are kept and <file>.new is written.
#   Files gone in the new version are deleted if unmodified, kept and reported if modified. -Force replaces modified files too.
# - remove refuses if another module's requires lists it or game code includes it (source/ for source modules; source/, sim/, view/, app/ including
#   mealkit/<name>/ for engine modules) (force with -Force). Undo is git's job.
param(
    [Parameter(Mandatory = $true, Position = 0)][ValidateSet("search", "list", "add", "update", "remove")][string]$Command,
    [Parameter(Position = 1)][string]$Target = "",
    [string]$Project = "",
    [string]$Name = "",
    [string]$From = "",
    [string]$To = "",
    [string]$Tag = "",
    [string]$Index = "",
    [string]$Preset = "",
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
$engineNamePattern = '^[a-z][a-z0-9_-]*(@\d+\.\d+\.\d+)?$'
$engineRoot = Split-Path -Parent $PSScriptRoot
$kernelNames = @("core", "platform", "jobs")

# ---------------------------------------------------------------- Common

function Read-ModuleInfo([string]$dir) {
    $info = @{ name = ""; version = "0"; engine = ""; requires = @(); type = "source"; title = ""; summary = ""; tags = ""; icon = "" }
    foreach ($line in [IO.File]::ReadAllLines((Join-Path $dir "module.txt"), $MealKitUtf8)) {
        if ($line -match '^\s*(name|version|engine|requires|type|title|summary|tags|icon)(?:\s+(.*?))?\s*$') {
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
    $engineType = (Read-ModuleInfo $dir).type -eq "engine"
    # The base is the long path from Get-Item (paths received as 8.3 short names like %TEMP% differ in length from FullName)
    $base = (Get-Item -LiteralPath $dir -Force).FullName.TrimEnd("\")
    Get-ChildItem -LiteralPath $base -Recurse -File -Force | Sort-Object FullName | ForEach-Object {
        $rel = $_.FullName.Substring($base.Length + 1).Replace("\", "/")
        $top = $rel.Split("/")[0]
        $keep = $copyTop -contains $top -or ($rel -notmatch "/" -and $top -match "^(license|readme)")
        if ($engineType) { $keep = $top -notin @(".git", ".github", "backends", "build") -and $_.Name -ne "CLAUDE.md" -and $rel -notmatch "^readme" -and $rel -ne $filesName }
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
# An engine module of this engine: modules/<name>/module.txt with "type engine" → its folder, else ""
function Find-EngineModule([string]$name) {
    $dir = Join-Path $engineRoot "modules\$name"
    $meta = Join-Path $dir "module.txt"
    if ((Test-Path $meta) -and (Read-ModuleInfo $dir).type -eq "engine") { return $dir }
    return ""
}

# modules.lock line of an engine module: @{ url; tag; commit; path } or $null (a module without its own repo yet, like audio)
function Get-EngineLock([string]$name) {
    $lock = Join-Path $engineRoot "modules.lock"
    if (-not (Test-Path $lock)) { return $null }
    foreach ($line in [IO.File]::ReadAllLines($lock)) {
        $p = $line.Trim() -split '\s+'
        if ($p.Count -ge 6 -and $p[0] -eq "module" -and $p[1] -eq $name) { return @{ url = $p[2]; tag = $p[3]; commit = $p[4]; path = $p[5] } }
    }
    return $null
}

function Get-EngineModuleSource([string]$name, [string]$version) {
    $vendored = Find-EngineModule $name
    $current = (Read-ModuleInfo $vendored).version
    $lock = Get-EngineLock $name
    if (-not $version -or $version -eq $current) {
        $tag = if ($lock) { "$($lock.tag) $($lock.commit) $($lock.url)" } else { "- - -" }
        return @{ dir = $vendored; source = "engine $name $current $tag" }
    }
    if (-not $lock) { throw "$name has no repo in the engine's modules.lock yet, so only its engine copy ($current) can be added" }
    # The checkout beside the engine (..\module-<name>) when there is one, else the lock's URL
    $repoName = ($lock.url.TrimEnd("/") -split "/")[-1]
    $sibling = Join-Path (Split-Path -Parent $engineRoot) $repoName
    $from = if (Test-Path (Join-Path $sibling ".git")) { $sibling } else { $lock.url }
    $cacheRoot = Join-Path ([Environment]::GetFolderPath("LocalApplicationData")) "MealKit\module-cache"
    $root = Join-Path $cacheRoot ([Guid]::NewGuid().ToString("N").Substring(0, 12))
    New-Item -ItemType Directory -Force $cacheRoot | Out-Null
    $ErrorActionPreference = "Continue"
    & git -c advice.detachedHead=false clone --quiet --depth 1 --branch "v$version" $from $root 2>&1 | ForEach-Object { Write-Host "  git: $_" }
    $cloned = $LASTEXITCODE -eq 0
    $ErrorActionPreference = "Stop"
    if (-not $cloned) { Remove-Item -Recurse -Force $root -ErrorAction SilentlyContinue; throw "$name has no version $version (tag v$version in $from)" }
    $commit = (& git -C $root rev-parse HEAD).Trim()
    $dir = if ($lock.path -eq ".") { $root } else { Join-Path $root $lock.path.Replace("/", "\") }
    if (-not (Test-Path (Join-Path $dir "module.txt"))) {
        Remove-Item -Recurse -Force $root -ErrorAction SilentlyContinue
        throw "$name $version is older than engine modules as module.txt (it has engine-module.txt): pick a newer version"
    }
    return @{ dir = $dir; source = "engine $name $version v$version $commit $($lock.url)" }
}

function Get-ModuleSource([string]$spec, [string]$want) {
    $root = ""
    $source = ""
    if (-not (Test-Path $spec) -and $spec -match $engineNamePattern -and (Find-EngineModule $spec.Split("@", 2)[0])) {
        $ename, $ever = $spec.Split("@", 2)
        return Get-EngineModuleSource $ename $ever
    }
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

# A module the project has: a folder in modules/, a kernel module, or an engine module when the project has no engine copies (it builds with all of them)
function Test-HasModule([string]$name) {
    if (Test-Path (Join-Path $modulesDir "$name\module.txt")) { return $true }
    if ($kernelNames -contains $name) { return $true }
    if ((Find-EngineModule $name) -and -not (Get-Installed | Where-Object { (Read-ModuleInfo $_.FullName).type -eq "engine" })) { return $true }
    return $false
}

function Test-ModuleWarnings([hashtable]$info) {
    if ($info.engine -and $info.engine -ne $engineMinor) {
        Write-Host "warning: $($info.name) $($info.version) is made for engine $($info.engine), this engine is $(Get-MealKitVersion)" -ForegroundColor Yellow
    }
    foreach ($r in $info.requires) {
        if (-not (Test-HasModule $r)) {
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
            #   installed <name> <version> <modified file count> <.new count> <source> <type> <title> <summary> <tags (space)> <icon>
            #   module <name> <version for this engine or -> <newest version> <engine of that version> <title> <summary> <tags (space)> <author> <repo> <icon>
            # (MealKit-Contract docs/CONTRACT.md "Module list"; engine modules carry "engine" first in their tags)
            #   error <message>   (the list couldn't be fetched. installed lines still appear)
            [Console]::OutputEncoding = New-Object Text.UTF8Encoding $false
            $clean = { param($s) "$s" -replace "[`t`r`n]", " " }
            foreach ($m in Get-Installed) {
                $st = Get-ModuleState $m.FullName
                $i = $st.info
                $itags = if ($i.type -eq "engine") { ("engine " + $i.tags).Trim() } else { $i.tags }
                $ititle = if ($i.title) { $i.title } else { $i.name }
                Write-Output (@("installed", $i.name, $i.version, $st.modified, $st.pending, (& $clean $st.source), $i.type, (& $clean $ititle),
                    (& $clean $i.summary), (& $clean $itags), (& $clean $i.icon)) -join "`t")
            }
            # The engine's own modules (type engine): one line each, like registry modules, with "engine" first in the tags
            foreach ($d in Get-ChildItem (Join-Path $engineRoot "modules") -Directory | Sort-Object Name) {
                if (-not (Find-EngineModule $d.Name) -or $d.Name -eq "editor") { continue }
                $ei = Read-ModuleInfo $d.FullName
                $lock = Get-EngineLock $d.Name
                $repo = if ($lock) { $lock.url } else { "-" }
                Write-Output (@("module", $ei.name, $ei.version, $ei.version, $engineMinor, $(if ($ei.title) { & $clean $ei.title } else { $ei.name }), (& $clean $ei.summary), ("engine " + $ei.tags).Trim(), "MealKit", $repo, (& $clean $ei.icon)) -join "`t")
            }
            try { $all = @((Get-Registry).modules) } catch { Write-Output "error`t$(& $clean $_.Exception.Message)"; $all = @() }
            foreach ($m in $all) {
                $fit = @($m.versions | Where-Object { $_.engine -eq $engineMinor })
                $first = @($m.versions)[0]
                Write-Output (@("module", $m.name, $(if ($fit.Count) { $fit[0].version } else { "-" }), $first.version, $first.engine,
                    (& $clean $m.title), (& $clean $m.summary), ($m.tags -join " "), (& $clean $m.author), $m.repo, (& $clean "$($m.icon)")) -join "`t")
            }
            break
        }
        $words = @("$Target".ToLowerInvariant() -split '\s+' | Where-Object { $_ })
        foreach ($d in Get-ChildItem (Join-Path $engineRoot "modules") -Directory | Sort-Object Name) {
            if (-not (Find-EngineModule $d.Name) -or $d.Name -eq "editor") { continue }
            $ei = Read-ModuleInfo $d.FullName
            $text = "$($ei.name) $($ei.summary) engine $($ei.tags)".ToLowerInvariant()
            if (($Tag -and "engine $($ei.tags)" -notmatch "\b$([regex]::Escape($Tag))\b") -or ($words | Where-Object { -not $text.Contains($_) })) { continue }
            $mark = if ($modulesDir -and (Test-Path (Join-Path $modulesDir "$($ei.name)\module.txt"))) { "  [installed]" } else { "" }
            Write-Host ("{0} {1}  engine module{2}" -f $ei.name, $ei.version, $mark)
            Write-Host "    $($ei.summary)"
        }
        try { $registryModules = @((Get-Registry).modules) } catch { Write-Host "warning: $($_.Exception.Message)" -ForegroundColor Yellow; $registryModules = @() }
        $hits = @($registryModules | Where-Object {
            $m = $_
            $text = "$($m.name) $($m.title) $($m.summary) $($m.tags -join ' ') $($m.author)".ToLowerInvariant()
            (-not $Tag -or $m.tags -contains $Tag) -and -not ($words | Where-Object { -not $text.Contains($_) })
        })
        if ($hits.Count -eq 0) { break }
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
            Write-Host ("{0} {1} ({2})  modified {3}, .new {4}  from {5}" -f $st.info.name, $st.info.version, $st.info.type, $st.modified, $st.pending, $st.source)
        }
    }

    "add" {
        if ($Preset) {
            # Every engine module of a preset (cmake/MealKit.cmake MEALKIT_PRESETS): all but editor; solo also without net and online*
            if ($Preset -notin @("standard", "solo")) { Write-Error "no preset '$Preset' (standard, solo)"; exit 1 }
            $names = @(Get-ChildItem (Join-Path $engineRoot "modules") -Directory | Sort-Object Name | ForEach-Object { $_.Name } |
                Where-Object { (Find-EngineModule $_) -and $_ -ne "editor" })
            if ($Preset -eq "solo") { $names = @($names | Where-Object { $_ -ne "net" -and $_ -ne "online" -and $_ -notlike "online-*" }) }
            $env:MEALKIT_MODULE_BATCH = "1" # requires are checked once at the end, not while half the preset is in
            foreach ($n in $names) {
                if (Test-Path (Join-Path $modulesDir "$n\module.txt")) { Write-Host "have $n"; continue }
                & $PSCommandPath add $n -Project $Project
                if ($LASTEXITCODE -ne 0) { exit 1 }
            }
            $env:MEALKIT_MODULE_BATCH = ""
            foreach ($m in Get-Installed) { Test-ModuleWarnings (Read-ModuleInfo $m.FullName) }
            break
        }
        if (-not $Target) { Write-Error "add needs a module name, a folder or a git address"; exit 1 }
        $hadEngine = @(Get-Installed | Where-Object { (Read-ModuleInfo $_.FullName).type -eq "engine" }).Count -gt 0
        $src = Get-ModuleSource $Target $Name
        try {
            $info = Read-ModuleInfo $src.dir
            $namePattern = if ($info.type -eq "engine") { '^[a-z0-9_-]+$' } else { '^[a-z0-9_]+$' }
            if ($info.name -notmatch $namePattern) { throw "module name '$($info.name)' must be lowercase letters, digits and _" }
            if ($info.type -eq "engine" -and $info.name -eq "editor") { throw "the editor module isn't part of a game" }
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
            if ($info.type -eq "engine" -and -not $hadEngine -and -not $env:MEALKIT_MODULE_BATCH) {
                Write-Host "Note: the game now builds with only the engine modules in modules/. Add the others it uses too (add -Preset standard adds all of them)." -ForegroundColor Yellow
            }
            $missing = @($info.requires | Where-Object { -not (Test-HasModule $_) })
            if ($WithRequires) {
                foreach ($req in $missing) {
                    Write-Host "Adding required module $req"
                    & $PSCommandPath add $req -Project $Project -Index $Index -WithRequires
                    if ($LASTEXITCODE -ne 0) { throw "could not add the required module $req" }
                }
            } elseif ($missing.Count -and -not $env:MEALKIT_MODULE_BATCH) {
                Write-Host "Also add: $($missing -join ', ') (or add again with -WithRequires)" -ForegroundColor Yellow
            }
            if (-not $env:MEALKIT_MODULE_BATCH) { Test-ModuleWarnings $info }
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
            elseif ($record.source -match '^(registry|engine)\s+(\S+)') { $spec = if ($To) { "$($Matches[2])@$To" } else { $Matches[2] } }
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
        $isEngine = (Read-ModuleInfo $dest).type -eq "engine"
        $codeDirs = if ($isEngine) { @("source", "sim", "view", "app") } else { @("source") }
        $pattern = if ($isEngine) { '#\s*include\s*["<]mealkit/' + [regex]::Escape($Target) + '/' } else { '#\s*include\s*["<]' + [regex]::Escape($Target) + '/' }
        foreach ($cd in $codeDirs) {
            $sourceDir = Join-Path $Project $cd
            if (-not (Test-Path $sourceDir)) { continue }
            # Code that keeps an engine module's uses inside #if MEALKIT_HAS_<NAME> already builds without it
            $guard = "MEALKIT_HAS_" + $Target.ToUpperInvariant().Replace("-", "_")
            Get-ChildItem $sourceDir -Recurse -File -Include *.h, *.hpp, *.cpp | Select-String -Pattern $pattern | ForEach-Object {
                if ($isEngine -and (Select-String -LiteralPath $_.Path -Pattern $guard -SimpleMatch -Quiet)) { return }
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

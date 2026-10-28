# 소스 모듈을 프로젝트 modules/<이름>/에 넣고, 갱신하고, 뺀다 (docs/MODULES.md). 에디터·MealKit Manager도 이것을 부른다.
#   powershell -ExecutionPolicy Bypass -File tools\module.ps1 search [<검색어>] [-Tag <태그>] [-Project <폴더>] [-Tsv]   (-Tsv: 에디터·Manager용 줄)
#   powershell -ExecutionPolicy Bypass -File tools\module.ps1 list   -Project <폴더>
#   powershell -ExecutionPolicy Bypass -File tools\module.ps1 add    -Project <폴더> <이름[@버전] | 폴더 | git주소[#태그]> [-Name <모듈>] [-WithRequires]
#   powershell -ExecutionPolicy Bypass -File tools\module.ps1 update -Project <폴더> <모듈> [-To <버전>] [-From <폴더 | git주소[#태그]>] [-DryRun] [-Force]
#   powershell -ExecutionPolicy Bypass -File tools\module.ps1 remove -Project <폴더> <모듈> [-Force]
#
# - 원본은 모듈 폴더(루트에 module.txt) 또는 그런 폴더를 가진 git 저장소. 저장소에 modules/<이름>/module.txt가 여럿이면 -Name으로 고른다.
#   이름은 레지스트리(MealKit-Modules) index.json에서 찾는다: 이 엔진 MINOR용 가장 새 버전을 태그로 받고, 커밋이 목록과 같은지 본다.
#   목록은 -Index 또는 환경 변수 MEALKIT_MODULE_INDEX(주소 또는 파일)로 바꾼다. 받은 목록은 %LOCALAPPDATA%\MealKit\module-index.json에 두고 오프라인이면 그것을 쓴다.
# - 복사하는 것: module.txt, sim/, view/, assets/, art/, docs/, 루트의 LICENSE*·README* (*.new 제외). 복사한 파일의 해시와 가져온 곳은 modules/<이름>/module.files에 남는다.
# - update는 upgrade_project.ps1과 같은 규칙: 고치지 않은 파일(해시가 기록과 같음)만 새것으로 바꾸고, 고친 파일은 남기고 <파일>.new를 쓴다.
#   새 버전에서 없어진 파일은 고치지 않았으면 지우고, 고쳤으면 남기고 알린다. -Force면 고친 파일도 바꾼다.
# - remove는 다른 모듈의 requires에 있거나 게임 source/가 include하면 거절한다(-Force로 강행). 되돌리기는 git이 맡는다.
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
if ($Command -ne "search" -or $Project) { # search는 프로젝트 없이도 된다 (있으면 설치 표시)
    if ($Project -eq "") { $Project = (Get-Location).Path }
    $Project = (Get-Item -LiteralPath (Resolve-Path $Project).Path -Force).FullName.TrimEnd("\") # 긴 경로 (8.3 짧은 이름 아님)
    if (-not (Test-Path (Join-Path $Project "mealkit.project"))) { Write-Error "$Project has no mealkit.project"; exit 1 }
    $modulesDir = Join-Path $Project "modules"
}
$filesName = "module.files"
# 복사하는 것은 모듈 구조뿐이다 (docs/MODULES.md): .git, .github, tests/, 빌드 폴더 등은 프로젝트에 가지 않는다
$copyTop = @("module.txt", "sim", "view", "assets", "art", "docs")
$textExtensions = @(".h", ".hpp", ".cpp", ".c", ".inl", ".txt", ".md", ".ui", ".table", ".tuning", ".scene", ".events", ".bindings",
    ".json", ".yml", ".yaml", ".gltf", ".cube", ".fx", ".gitignore", ".gitattributes")
$engineMinor = ((Get-MealKitVersion) -replace '^(\d+\.\d+).*$', '$1')
$defaultIndex = "https://raw.githubusercontent.com/MealKit-Engine/MealKit-Modules/main/index.json"
$registryNamePattern = '^[a-z][a-z0-9_]*(@\d+\.\d+\.\d+)?$'

# ---------------------------------------------------------------- 공통

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

# 줄바꿈과 무관한 해시 (텍스트), 바이트 해시 (그 밖)
function Get-ModuleFileHash([string]$file) {
    if ($textExtensions -contains [IO.Path]::GetExtension($file).ToLowerInvariant()) {
        return Get-TextHash ([IO.File]::ReadAllText($file, $MealKitUtf8))
    }
    $sha = [Security.Cryptography.SHA256]::Create()
    return ([BitConverter]::ToString($sha.ComputeHash([IO.File]::ReadAllBytes($file)))).Replace("-", "").ToLowerInvariant()
}

# 모듈 폴더의 복사 대상 파일: 모듈 기준 경로(/) → 전체 경로
function Get-ModuleFiles([string]$dir) {
    $files = [ordered]@{}
    # 기준은 Get-Item이 준 긴 경로 (%TEMP%처럼 8.3 짧은 이름으로 받은 경로와 FullName의 길이가 다르다)
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

# 레지스트리 목록 (MealKit-Modules index.json). 받지 못하면 지난번 받은 캐시
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

# 고를 버전: 정한 버전, 아니면 이 엔진 MINOR용 가장 새 버전, 그것도 없으면 가장 새 버전(경고)
function Select-RegistryVersion($module, [string]$version) {
    $versions = @($module.versions) # build_index.py가 새 버전부터 적는다
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

# 원본(레지스트리 이름[@버전], 폴더, git 주소#태그) → @{ dir; source } . git은 캐시 폴더에 새로 받는다
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
        # git은 알림도 stderr로 쓴다: Stop이면 PowerShell 5.1이 그것을 오류로 끊으므로 종료 코드로만 판단한다
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

# 설치된 모듈 하나: module.txt, 기록과 다른(고친) 파일 수, 남은 .new 수, 출처
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

# ---------------------------------------------------------------- 명령

switch ($Command) {
    "search" {
        if ($Tsv) {
            # 에디터 [모듈] 창과 MealKit Manager [모듈] 탭이 읽는 줄 (탭으로 나눔, UTF-8). 한 번 불러 설치된 것과 목록을 같이 받는다:
            #   installed <이름> <버전> <고친 파일 수> <.new 수> <출처>
            #   module <이름> <이 엔진용 버전 또는 -> <가장 새 버전> <그 버전의 엔진> <제목> <요약> <태그(공백)> <작성자> <저장소>
            #   error <문구>   (목록을 받지 못했다. installed 줄은 그대로 나온다)
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
        # 이 엔진 MINOR용 버전이 있는 것 먼저
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
                    $hashes[$rel] = $recorded # 모듈은 그대로이고 게임만 고쳤다
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

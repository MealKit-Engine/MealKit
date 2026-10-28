# 소스 모듈을 프로젝트 modules/<이름>/에 넣고, 갱신하고, 뺀다 (docs/MODULES.md). 에디터·MealKit Manager도 이것을 부른다.
#   powershell -ExecutionPolicy Bypass -File tools\module.ps1 list   -Project <폴더>
#   powershell -ExecutionPolicy Bypass -File tools\module.ps1 add    -Project <폴더> <폴더 | git주소[#태그]> [-Name <모듈>]
#   powershell -ExecutionPolicy Bypass -File tools\module.ps1 update -Project <폴더> <모듈> [-From <폴더 | git주소[#태그]>] [-DryRun] [-Force]
#   powershell -ExecutionPolicy Bypass -File tools\module.ps1 remove -Project <폴더> <모듈> [-Force]
#
# - 원본은 모듈 폴더(루트에 module.txt) 또는 그런 폴더를 가진 git 저장소. 저장소에 modules/<이름>/module.txt가 여럿이면 -Name으로 고른다.
#   레지스트리 이름으로 찾기(MealKit-Modules)는 아직 없다.
# - 복사하는 것: module.txt, sim/, view/, assets/, art/, docs/, 루트의 LICENSE*·README* (*.new 제외). 복사한 파일의 해시와 가져온 곳은 modules/<이름>/module.files에 남는다.
# - update는 upgrade_project.ps1과 같은 규칙: 고치지 않은 파일(해시가 기록과 같음)만 새것으로 바꾸고, 고친 파일은 남기고 <파일>.new를 쓴다.
#   새 버전에서 없어진 파일은 고치지 않았으면 지우고, 고쳤으면 남기고 알린다. -Force면 고친 파일도 바꾼다.
# - remove는 다른 모듈의 requires에 있거나 게임 source/가 include하면 거절한다(-Force로 강행). 되돌리기는 git이 맡는다.
param(
    [Parameter(Mandatory = $true, Position = 0)][ValidateSet("list", "add", "update", "remove")][string]$Command,
    [Parameter(Position = 1)][string]$Target = "",
    [string]$Project = "",
    [string]$Name = "",
    [string]$From = "",
    [switch]$DryRun,
    [switch]$Force
)
$ErrorActionPreference = "Stop"
. "$PSScriptRoot\project_common.ps1"

if ($Project -eq "") { $Project = (Get-Location).Path }
$Project = (Get-Item -LiteralPath (Resolve-Path $Project).Path -Force).FullName.TrimEnd("\") # 긴 경로 (8.3 짧은 이름 아님)
if (-not (Test-Path (Join-Path $Project "mealkit.project"))) { Write-Error "$Project has no mealkit.project"; exit 1 }
$modulesDir = Join-Path $Project "modules"
$filesName = "module.files"
# 복사하는 것은 모듈 구조뿐이다 (docs/MODULES.md): .git, .github, tests/, 빌드 폴더 등은 프로젝트에 가지 않는다
$copyTop = @("module.txt", "sim", "view", "assets", "art", "docs")
$textExtensions = @(".h", ".hpp", ".cpp", ".c", ".inl", ".txt", ".md", ".ui", ".table", ".tuning", ".scene", ".events", ".bindings",
    ".json", ".yml", ".yaml", ".gltf", ".cube", ".fx", ".gitignore", ".gitattributes")
$engineMinor = ((Get-MealKitVersion) -replace '^(\d+\.\d+).*$', '$1')

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

# 원본(폴더 또는 git 주소#태그) → @{ dir; source } . git은 캐시 폴더에 새로 받는다
function Get-ModuleSource([string]$spec, [string]$want) {
    $root = ""
    $source = ""
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
        throw "'$spec' is not a folder or a git address. (Adding by registry name comes with MealKit-Modules.)"
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

function Get-Installed {
    if (-not (Test-Path $modulesDir)) { return @() }
    return @(Get-ChildItem $modulesDir -Directory | Where-Object { Test-Path (Join-Path $_.FullName "module.txt") } | Sort-Object Name)
}

# ---------------------------------------------------------------- 명령

switch ($Command) {
    "list" {
        $installed = Get-Installed
        if ($installed.Count -eq 0) { Write-Host "No modules in $modulesDir"; break }
        foreach ($m in $installed) {
            $info = Read-ModuleInfo $m.FullName
            $record = Read-FilesRecord $m.FullName
            $modified = 0
            foreach ($rel in $record.hashes.Keys) {
                $file = Join-Path $m.FullName $rel
                if (-not (Test-Path $file) -or (Get-ModuleFileHash $file) -ne $record.hashes[$rel]) { $modified++ }
            }
            $pending = @(Get-ChildItem $m.FullName -Recurse -File -Filter *.new).Count
            $source = if ($record.source) { $record.source } else { "(no module.files)" }
            Write-Host ("{0} {1}  modified {2}, .new {3}  from {4}" -f $info.name, $info.version, $modified, $pending, $source)
        }
    }

    "add" {
        if (-not $Target) { Write-Error "add needs a folder or git address"; exit 1 }
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

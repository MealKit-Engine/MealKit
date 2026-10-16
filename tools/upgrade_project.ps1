# 게임 프로젝트를 지금 엔진의 템플릿으로 올린다.
#   powershell -ExecutionPolicy Bypass -File tools\upgrade_project.ps1 -Project <프로젝트 폴더> [-DryRun] [-Force] [-Source]
#
# - 템플릿이 관리하는 파일(build.bat, editor.bat, CMakeLists.txt, CLAUDE.md, README.md, .vscode, .clang-format 등)만 다룬다.
#   source/와 assets/는 건드리지 않는다 (-Source를 줄 때만 예외).
# - 사용자가 고치지 않은 파일(.mealkit-template의 해시와 같음)은 새 템플릿으로 바꾼다. 고친 파일은 덮어쓰지 않고
#   새 내용을 <파일>.new로 남겨 차이를 보여 준다. -Force면 고친 파일도 바꾼다.
# - mealkit.project는 engine과 engineVersion 줄만 고친다 (이름·시작 씬·게임 항목은 그대로, 줄 형식도 그대로).
# - 바꾸기 전 파일은 .upgrade_backup\<시각>\에 남긴다.
# - -Source: source/ 게임 코드를 엔진 예제 코드(samples/sandbox)로 바꾼다. 템플릿 코드를 고치지 않은 프로젝트를 새 엔진 구조로
#   옮길 때 쓴다. 예제에 없는 source/sim·view·app 파일은 지운다(백업). assets/에는 없는 파일만 더한다(덮어쓰지 않음).
param(
    [Parameter(Mandatory = $true)][string]$Project,
    [switch]$DryRun,
    [switch]$Force,
    [switch]$Source
)
$ErrorActionPreference = "Stop"
. "$PSScriptRoot\project_common.ps1"

$Project = (Resolve-Path $Project).Path.TrimEnd("\")
if (-not (Test-Path (Join-Path $Project "mealkit.project"))) { Write-Error "$Project has no mealkit.project"; exit 1 }
if ($Project -ieq $MealKitEngine) { Write-Error "This is the engine repository itself - nothing to upgrade"; exit 1 }

$info = Read-ProjectFile $Project
$name = $info["name"]
if (-not $name) { Write-Error "mealkit.project has no name"; exit 1 }
$version = Get-MealKitVersion
$fromVersion = if ($info.Contains("engineVersion")) { $info["engineVersion"] } else { "(none)" }
Write-Host "Upgrading '$name' ($Project): engine $fromVersion -> $version$(if ($DryRun) { '  [dry run]' })"

$manifest = Read-TemplateManifest $Project
$newManifest = @{}
$backup = Join-Path $Project (".upgrade_backup\" + (Get-Date -Format "yyyyMMdd-HHmmss"))
$changed = 0
$kept = 0

function Backup([string]$rel) {
    $src = Join-Path $Project $rel
    if (-not (Test-Path $src)) { return }
    $dst = Join-Path $backup $rel
    New-Item -ItemType Directory -Force (Split-Path -Parent $dst) | Out-Null
    Copy-Item $src $dst -Force
}

function Report([string]$status, [string]$rel, [ConsoleColor]$color = "Gray") {
    Write-Host ("  {0,-9} {1}" -f $status, $rel) -ForegroundColor $color
}

# ---------------------------------------------------------------- 템플릿 파일
foreach ($entry in (Get-RenderedTemplate $name).GetEnumerator()) {
    $rel = $entry.Key
    $new = $entry.Value
    $target = Join-Path $Project $rel

    if ($rel -eq "mealkit.project") {
        # 사용자 값(name, scene, game.* 등)은 두고 엔진 경로와 버전만 바꾼다.
        # 예전 "키=값" 줄은 그 형식 그대로 둔다 (고친 build.bat이 예전 형식만 읽을 수 있다).
        $current = [IO.File]::ReadAllText($target, $MealKitUtf8)
        $lines = [Collections.Generic.List[string]]::new()
        $seenEngine = $false; $seenVersion = $false; $separator = " "
        foreach ($line in ($current -split "`r?`n")) {
            if ($line -match '^\s*(engine|engineVersion)(\s*=\s*|\s+|$)') {
                $sep = if ($Matches[2] -match '=') { "=" } else { " " }
                $separator = $sep
                if ($Matches[1] -eq "engine") { $lines.Add("engine$sep$MealKitEngine"); $seenEngine = $true }
                else { $lines.Add("engineVersion$sep$version"); $seenVersion = $true }
            }
            else { $lines.Add($line) }
        }
        while ($lines.Count -gt 0 -and $lines[$lines.Count - 1] -eq "") { $lines.RemoveAt($lines.Count - 1) }
        if (-not $seenEngine) { $lines.Add("engine$separator$MealKitEngine") }
        if (-not $seenVersion) { $lines.Add("engineVersion$separator$version") }
        $merged = ($lines -join "`r`n") + "`r`n"
        if ($merged.Replace("`r`n", "`n") -ne $current.Replace("`r`n", "`n")) {
            Report "updated" $rel Green
            if (-not $DryRun) { Backup $rel; Write-TextFile $target $merged }
            $changed++
        } else { Report "same" $rel }
        continue
    }

    $newHash = Get-TextHash $new
    if (-not (Test-Path $target)) {
        Report "added" $rel Green
        if (-not $DryRun) { Write-TextFile $target $new }
        $newManifest[$rel] = $newHash
        $changed++
        continue
    }
    $current = [IO.File]::ReadAllText($target, $MealKitUtf8)
    $currentHash = Get-TextHash $current
    if ($currentHash -eq $newHash) {
        Report "same" $rel
        $newManifest[$rel] = $newHash
    } elseif ($Force -or ($manifest.Contains($rel) -and $manifest[$rel] -eq $currentHash)) {
        Report "updated" $rel Green
        if (-not $DryRun) { Backup $rel; Write-TextFile $target $new }
        $newManifest[$rel] = $newHash
        $changed++
    } elseif ($manifest.Contains($rel) -and $manifest[$rel] -eq $newHash) {
        # 템플릿은 그대로이고 사용자만 고쳤다: 할 일 없음
        Report "custom" $rel
        $newManifest[$rel] = $manifest[$rel]
    } else {
        # 사용자가 고친 파일 (또는 해시 기록이 없는 예전 프로젝트): 덮어쓰지 않고 새 내용을 옆에 둔다
        $diff = Compare-Object ($current -split "`r?`n") ($new -split "`r?`n")
        Report "modified" "$rel  (kept; new template in $rel.new, $($diff.Count) line(s) differ. -Force to replace)" Yellow
        if (-not $DryRun) { Write-TextFile "$target.new" $new }
        if ($manifest.Contains($rel)) { $newManifest[$rel] = $manifest[$rel] }
        $kept++
    }
}

# ---------------------------------------------------------------- 게임 코드 (-Source)
if ($Source) {
    $sample = Get-SampleSource $name
    foreach ($dir in "sim", "view", "app") {
        $folder = Join-Path $Project "source\$dir"
        if (-not (Test-Path $folder)) { continue }
        Get-ChildItem $folder -Recurse -File | ForEach-Object {
            $rel = $_.FullName.Substring($Project.Length + 1)
            if (-not $sample.Contains($rel)) {
                Report "removed" $rel Yellow
                if (-not $DryRun) { Backup $rel; Remove-Item $_.FullName -Force }
                $changed++
            }
        }
    }
    foreach ($entry in $sample.GetEnumerator()) {
        $target = Join-Path $Project $entry.Key
        if ((Test-Path $target) -and (Get-TextHash ([IO.File]::ReadAllText($target, $MealKitUtf8))) -eq (Get-TextHash $entry.Value)) {
            continue
        }
        Report $(if (Test-Path $target) { "updated" } else { "added" }) $entry.Key Green
        if (-not $DryRun) { Backup $entry.Key; Write-TextFile $target $entry.Value }
        $changed++
    }
    # 예제가 쓰는 에셋 중 프로젝트에 없는 것만 더한다 (예: assets/game.tuning)
    $assets = Join-Path $MealKitEngine "assets"
    Get-ChildItem $assets -Recurse -File | Where-Object { $_.Extension -ne ".autosave" } | ForEach-Object {
        $rel = "assets\" + $_.FullName.Substring($assets.Length + 1)
        $target = Join-Path $Project $rel
        if (-not (Test-Path $target)) {
            Report "added" $rel Green
            if (-not $DryRun) {
                New-Item -ItemType Directory -Force (Split-Path -Parent $target) | Out-Null
                Copy-Item $_.FullName $target
            }
            $changed++
        }
    }
}

if (-not $DryRun) { Write-TemplateManifest $Project $newManifest }
Write-Host "Done: $changed change(s), $kept locally modified file(s) kept$(if ((-not $DryRun) -and $changed -gt 0) { ". Backup: $backup" })"
if ($changed -gt 0 -and -not $DryRun) { Write-Host "Rebuild the game code: $Project\build.bat Editor" }
exit 0

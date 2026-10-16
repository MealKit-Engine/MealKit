# 게임 프로젝트 이름(= exe 이름, CMake 타깃 이름) 바꾸기. 에디터 프로젝트 관리자가 이름 칸을 고치면 실행한다.
#   powershell -ExecutionPolicy Bypass -File tools\rename_project.ps1 -Project <폴더> -Name <새 이름>
# - mealkit.project의 name 줄 (원래 형식 그대로)
# - 템플릿 파일(CMakeLists.txt, build.bat, README.md, CLAUDE.md, .vscode): 고치지 않았으면 새 이름으로 다시 채우고,
#   고쳤으면 옛 이름(앞뒤가 영문·숫자가 아닌 곳)만 바꾼다. 고치지 않은 파일은 .mealkit-template 해시도 새로 적는다.
# - source\ 코드: 새 프로젝트를 만들 때 넣은 곳만 ("옛 이름" 문자열, 옛이름.exe)
# - build\ 를 지운다 (CMake 캐시와 산출물에 옛 이름이 들어 있다). export\<옛 이름>은 그대로 둔다
param(
    [Parameter(Mandatory = $true)][string]$Project,
    [Parameter(Mandatory = $true)][string]$Name
)
$ErrorActionPreference = "Stop"
. "$PSScriptRoot\project_common.ps1"

if ($Name -notmatch '^[A-Za-z][A-Za-z0-9_]*$') {
    Write-Error "Name must start with a letter and use only letters, digits and '_' (it becomes the exe name)"
    exit 1
}
$Project = (Resolve-Path $Project).Path.TrimEnd("\")
if ($Project -ieq $MealKitEngine) {
    Write-Error "The engine's sample project cannot be renamed (samples/sandbox is the template)"
    exit 1
}
$projectFile = Join-Path $Project "mealkit.project"
if (-not (Test-Path $projectFile)) {
    Write-Error "$projectFile not found"
    exit 1
}
$old = (Read-ProjectFile $Project)["name"]
if (-not $old) {
    Write-Error "mealkit.project has no name"
    exit 1
}
if ($old -ceq $Name) {
    Write-Host "Name is already $Name"
    exit 0
}
$word = '(?<![A-Za-z0-9])' + [regex]::Escape($old) + '(?![A-Za-z0-9])'

# 1) mealkit.project: name 줄만 (원래 형식 그대로)
$lines = [IO.File]::ReadAllLines($projectFile, $MealKitUtf8)
for ($i = 0; $i -lt $lines.Length; $i++) {
    if ($lines[$i] -match '^(\s*name)(\s*=\s*|\s+)(.*?)\s*$') { $lines[$i] = $Matches[1] + $Matches[2] + $Name }
}
Write-TextFile $projectFile (($lines -join "`r`n") + "`r`n")
Write-Host "mealkit.project: name $old -> $Name"

# 2) 템플릿 파일
$manifest = Read-TemplateManifest $Project
$oldRender = Get-RenderedTemplate $old
$newRender = Get-RenderedTemplate $Name
foreach ($key in $newRender.Keys) {
    if ($key -eq "mealkit.project") { continue }
    $file = Join-Path $Project $key
    if (-not (Test-Path $file)) { continue }
    $text = [IO.File]::ReadAllText($file, $MealKitUtf8)
    $hash = Get-TextHash $text
    $unmodified = $manifest.ContainsKey($key) -and $manifest[$key] -eq $hash
    if ($hash -eq (Get-TextHash $oldRender[$key])) { $newText = $newRender[$key] }
    else { $newText = [regex]::Replace($text, $word, $Name) }
    if ($newText -ne $text) {
        Write-TextFile $file $newText
        $note = if ($unmodified) { "" } else { " (edited file: check it)" }
        Write-Host "  $key$note"
    }
    if ($unmodified) { $manifest[$key] = Get-TextHash $newText }
}
if ($manifest.Count -gt 0) { Write-TemplateManifest $Project $manifest }

# 3) 게임 코드: new_project가 넣은 이름(창 제목, 도움말, 주석). 코드 식별자는 건드리지 않도록 문자열과 주석 안만 바꾼다
$source = Join-Path $Project "source"
if (Test-Path $source) {
    $source = (Get-Item $source).FullName
    $wordInCode = '(?<![A-Za-z0-9_])' + [regex]::Escape($old) + '(?![A-Za-z0-9_])'
    $stringOrComment = '"(?:[^"\\\r\n]|\\.)*"|//[^\r\n]*'
    Get-ChildItem $source -Recurse -File | Where-Object { $_.Extension -in ".h", ".cpp" } | ForEach-Object {
        $text = [IO.File]::ReadAllText($_.FullName, $MealKitUtf8)
        $newText = [regex]::Replace($text, $stringOrComment, { param($m) [regex]::Replace($m.Value, $wordInCode, $Name) })
        if ($newText -ne $text) {
            Write-TextFile $_.FullName $newText
            Write-Host ("  source\" + $_.FullName.Substring($source.Length + 1))
        }
    }
}

# 4) 빌드 폴더 (다음 빌드에서 새 이름으로 다시 만든다)
$build = Join-Path $Project "build"
if (Test-Path $build) {
    Remove-Item -Recurse -Force $build
    Write-Host "Removed build\ (rebuilt on next open)"
}
if (Test-Path (Join-Path $Project "export\$old")) { Write-Host "export\$old was kept (old release build)" }
Write-Host "Renamed $old -> $Name" -ForegroundColor Green

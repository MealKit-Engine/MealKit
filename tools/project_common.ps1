# new_project.ps1과 upgrade_project.ps1이 같이 쓰는 함수 (dot-source: . "$PSScriptRoot\project_common.ps1")
# 템플릿(templates/project)을 채워 만든 파일은 .mealkit-template에 SHA-256을 남긴다.
# 업그레이드는 이 값과 같으면(사용자가 고치지 않았으면) 새 템플릿으로 바꾸고, 다르면 덮어쓰지 않는다.

$MealKitEngine = (Resolve-Path (Split-Path -Parent $PSScriptRoot)).Path
$MealKitUtf8 = New-Object System.Text.UTF8Encoding $false
$MealKitManifestName = ".mealkit-template"

function Get-MealKitVersion {
    return ([IO.File]::ReadAllText((Join-Path $MealKitEngine "VERSION"))).Trim()
}

function Get-TemplateTokens([string]$name) {
    return @{
        "@NAME@"             = $name
        "@MEALKIT_ROOT@"     = $MealKitEngine
        "@MEALKIT_ROOT_FWD@" = $MealKitEngine.Replace("\", "/")
        "@ENGINE_VERSION@"   = (Get-MealKitVersion)
        "@CREATED@"          = (Get-Date -Format "yyyy-MM-dd") # mealkit.project에만 쓴다 (해시하지 않는 파일)
    }
}

# 템플릿 파일을 채운 내용: 프로젝트 기준 상대 경로 → 텍스트
function Get-RenderedTemplate([string]$name) {
    $tokens = Get-TemplateTokens $name
    $template = Join-Path $MealKitEngine "templates\project"
    $files = [ordered]@{}
    Get-ChildItem $template -Recurse -File -Force | Sort-Object FullName | ForEach-Object {
        $text = [IO.File]::ReadAllText($_.FullName, $MealKitUtf8)
        foreach ($k in $tokens.Keys) { $text = $text.Replace($k, $tokens[$k]) }
        $files[$_.FullName.Substring($template.Length + 1)] = $text
    }
    return $files
}

# 줄바꿈(CRLF/LF)과 무관한 내용 해시
function Get-TextHash([string]$text) {
    $bytes = $MealKitUtf8.GetBytes($text.Replace("`r`n", "`n"))
    $sha = [Security.Cryptography.SHA256]::Create()
    return ([BitConverter]::ToString($sha.ComputeHash($bytes))).Replace("-", "").ToLowerInvariant()
}

function Write-TextFile([string]$file, [string]$text) {
    New-Item -ItemType Directory -Force (Split-Path -Parent $file) | Out-Null
    [IO.File]::WriteAllText($file, $text, $MealKitUtf8)
}

function Read-TemplateManifest([string]$project) {
    $map = @{}
    $file = Join-Path $project $MealKitManifestName
    if (Test-Path $file) {
        foreach ($line in [IO.File]::ReadAllLines($file, $MealKitUtf8)) {
            if ($line -match '^([0-9a-f]{64})\s+(.+)$') { $map[$Matches[2]] = $Matches[1] }
        }
    }
    return $map
}

function Write-TemplateManifest([string]$project, [hashtable]$map) {
    $lines = @("# MealKit template files (sha256  path). tools\upgrade_project.ps1 replaces a file only while its hash matches.")
    foreach ($k in ($map.Keys | Sort-Object)) { $lines += "$($map[$k])  $k" }
    Write-TextFile (Join-Path $project $MealKitManifestName) (($lines -join "`r`n") + "`r`n")
}

# mealkit.project 한 줄: "경로 값" 또는 예전 형식 "키=값" (mealkit/game/ProjectSettings.cpp와 같은 규칙)
$MealKitProjectLine = '^\s*([A-Za-z_][A-Za-z0-9_.\[\]]*)(?:\s*=\s*|\s+|$)(.*?)\s*$'

# mealkit.project 읽기 (경로 → 값)
function Read-ProjectFile([string]$project) {
    $values = [ordered]@{}
    foreach ($line in [IO.File]::ReadAllLines((Join-Path $project "mealkit.project"), $MealKitUtf8)) {
        if ($line -match '^\s*#') { continue }
        if ($line -match $MealKitProjectLine) { $values[$Matches[1]] = $Matches[2] }
    }
    return $values
}

# 예제 게임 코드(samples/sandbox의 sim/ view/ app/)를 프로젝트 source/로 옮길 때의 내용. 창 제목과 도움말의 이름만 바꾼다.
function Get-SampleSource([string]$name) {
    $sample = Join-Path $MealKitEngine "samples\sandbox"
    $files = [ordered]@{}
    foreach ($dir in "sim", "view", "app") {
        $from = Join-Path $sample $dir
        Get-ChildItem $from -Recurse -File | Where-Object { $_.Extension -in ".h", ".cpp" } | Sort-Object FullName | ForEach-Object {
            $text = [IO.File]::ReadAllText($_.FullName, $MealKitUtf8)
            $text = $text.Replace("MealKit Sandbox", $name).Replace("sandbox.exe", "$name.exe")
            $files["source\$dir\" + $_.FullName.Substring($from.Length + 1)] = $text
        }
    }
    return $files
}

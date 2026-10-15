# 새 게임 프로젝트 만들기: 엔진 밖의 폴더에 게임 코드 + 에셋 + 빌드 파일을 만든다.
# 게임 코드와 에셋은 예제 게임(samples/sandbox, assets/)에서 복사하고, 빌드 파일은 templates/project를 채워 만든다.
#   powershell -ExecutionPolicy Bypass -File tools\new_project.ps1 -Name MyGame [-Path D:\GameEngine\MealKit]
# 결과: <Path>\<Name>\ (build.bat, editor.bat, source\, assets\). 엔진은 이 폴더(스크립트의 상위 폴더)를 가리킨다.
param(
    [Parameter(Mandatory = $true)][string]$Name,
    [string]$Path = "D:\GameEngine\MealKit"
)
$ErrorActionPreference = "Stop"

# 이름은 exe 이름과 CMake 타깃 이름이 된다
if ($Name -notmatch '^[A-Za-z][A-Za-z0-9_]*$') {
    Write-Error "Name must start with a letter and use only letters, digits and '_' (it becomes the exe name)"
    exit 1
}
$engine = (Resolve-Path (Split-Path -Parent $PSScriptRoot)).Path
$dest = Join-Path ([IO.Path]::GetFullPath($Path)) $Name
if ($dest.StartsWith($engine + "\", [StringComparison]::OrdinalIgnoreCase)) {
    Write-Error "Create projects outside the engine folder ($engine)"
    exit 1
}
if ((Test-Path $dest) -and (Get-ChildItem $dest -Force | Select-Object -First 1)) {
    Write-Error "$dest already exists and is not empty"
    exit 1
}

$utf8 = New-Object System.Text.UTF8Encoding $false
$tokens = @{
    "@NAME@"           = $Name
    "@MEALKIT_ROOT@"     = $engine
    "@MEALKIT_ROOT_FWD@" = $engine.Replace("\", "/")
}

function Write-Text([string]$file, [string]$text) {
    New-Item -ItemType Directory -Force (Split-Path -Parent $file) | Out-Null
    [IO.File]::WriteAllText($file, $text, $utf8)
}

# 1) 빌드 파일: templates/project 를 토큰을 바꿔 복사
$template = Join-Path $engine "templates\project"
Get-ChildItem $template -Recurse -File -Force | ForEach-Object {
    $text = [IO.File]::ReadAllText($_.FullName, $utf8)
    foreach ($k in $tokens.Keys) { $text = $text.Replace($k, $tokens[$k]) }
    Write-Text (Join-Path $dest $_.FullName.Substring($template.Length + 1)) $text
}

# 2) 게임 코드: 예제 게임에서 복사. 창 제목과 도움말의 이름만 바꾼다 (namespace sandbox는 그대로 둔다)
$sample = Join-Path $engine "samples\sandbox"
Get-ChildItem $sample -File | Where-Object { $_.Extension -in ".h", ".cpp" } | ForEach-Object {
    $text = [IO.File]::ReadAllText($_.FullName, $utf8)
    $text = $text.Replace("MealKit Sandbox", $Name).Replace("sandbox.exe", "$Name.exe")
    Write-Text (Join-Path $dest "source\$($_.Name)") $text
}

# 3) 에셋: 씬과 모델 (자동 저장 파일 제외)
$assets = Join-Path $engine "assets"
Get-ChildItem $assets -Recurse -File | Where-Object { $_.Extension -ne ".autosave" } | ForEach-Object {
    $target = Join-Path $dest ("assets\" + $_.FullName.Substring($assets.Length + 1))
    New-Item -ItemType Directory -Force (Split-Path -Parent $target) | Out-Null
    Copy-Item $_.FullName $target
}

Write-Host "Created $dest" -ForegroundColor Green
Write-Host "  engine : $engine"
Write-Host "  build  : $dest\build.bat        (first build compiles the engine too, a few minutes)"
Write-Host "  editor : $dest\editor.bat"
Write-Host "  export : $dest\build.bat Export  -> $dest\export\$Name"

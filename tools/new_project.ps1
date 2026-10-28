# 새 게임 프로젝트 만들기: 엔진 밖의 폴더에 게임 코드 + 에셋 + 빌드 파일을 만든다.
# 게임 코드와 에셋은 예제 게임(samples/sandbox, assets/)에서 복사하고, 빌드 파일은 templates/project를 채워 만든다.
#   powershell -ExecutionPolicy Bypass -File tools\new_project.ps1 -Name MyGame [-Path D:\GameEngine\MealKit]
# 결과: <Path>\<Name>\ (build.bat, editor.bat, source\sim|view|app\, assets\). 엔진은 이 폴더(스크립트의 상위 폴더)를 가리킨다.
# 템플릿 파일의 해시는 .mealkit-template에 남는다 (나중에 tools\upgrade_project.ps1이 고치지 않은 파일만 새 템플릿으로 바꾼다).
param(
    [Parameter(Mandatory = $true)][string]$Name,
    [string]$Path = "D:\GameEngine\MealKit"
)
$ErrorActionPreference = "Stop"
. "$PSScriptRoot\project_common.ps1"

# 이름은 exe 이름과 CMake 타깃 이름이 된다
if ($Name -notmatch '^[A-Za-z][A-Za-z0-9_]*$') {
    Write-Error "Name must start with a letter and use only letters, digits and '_' (it becomes the exe name)"
    exit 1
}
$engine = $MealKitEngine
$dest = Join-Path ([IO.Path]::GetFullPath($Path)) $Name
if ($dest.StartsWith($engine + "\", [StringComparison]::OrdinalIgnoreCase)) {
    Write-Error "Create projects outside the engine folder ($engine)"
    exit 1
}
if ((Test-Path $dest) -and (Get-ChildItem $dest -Force | Select-Object -First 1)) {
    Write-Error "$dest already exists and is not empty"
    exit 1
}

# 1) 빌드 파일: templates/project 를 토큰을 바꿔 복사하고 해시를 남긴다
$manifest = @{}
foreach ($entry in (Get-RenderedTemplate $Name).GetEnumerator()) {
    Write-TextFile (Join-Path $dest $entry.Key) $entry.Value
    if ($entry.Key -ne "mealkit.project") { $manifest[$entry.Key] = Get-TextHash $entry.Value }
}
Write-TemplateManifest $dest $manifest

# 2) 게임 코드: 예제 게임의 sim/ view/ app/ 을 source/ 로 복사 (namespace sandbox는 그대로 둔다)
foreach ($entry in (Get-SampleSource $Name).GetEnumerator()) { Write-TextFile (Join-Path $dest $entry.Key) $entry.Value }

# 3) 에셋: 씬, 모델, 튜닝 (자동 저장 파일 제외)
$assets = Join-Path $engine "assets"
Get-ChildItem $assets -Recurse -File | Where-Object { $_.Extension -ne ".autosave" } | ForEach-Object {
    $target = Join-Path $dest ("assets\" + $_.FullName.Substring($assets.Length + 1))
    New-Item -ItemType Directory -Force (Split-Path -Parent $target) | Out-Null
    Copy-Item $_.FullName $target
}

# 4) 소스 모듈: 예제 게임이 쓰는 modules/<이름>/ (module.files 설치 기록째, 이후 tools\module.ps1 update로 갱신)
$modules = Join-Path $engine "modules"
if (Test-Path $modules) {
    New-Item -ItemType Directory -Force (Join-Path $dest "modules") | Out-Null
    Get-ChildItem $modules -Directory | ForEach-Object { Copy-Item $_.FullName (Join-Path $dest "modules") -Recurse -Force }
}

Write-Host "Created $dest (engine $(Get-MealKitVersion))" -ForegroundColor Green
Write-Host "  engine : $engine"
Write-Host "  build  : $dest\build.bat        (first build compiles the engine too, a few minutes)"
Write-Host "  editor : $dest\editor.bat"
Write-Host "  export : $dest\build.bat Export  -> $dest\export\$Name"

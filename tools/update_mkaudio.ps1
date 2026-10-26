# 오디오 런타임 mkaudio(별도 저장소 MealKit-Audio)를 엔진 third_party/mkaudio/로 가져온다 (docs/CODING_MODEL.md "외부 라이브러리 두기").
#   powershell -ExecutionPolicy Bypass -File tools\update_mkaudio.ps1 -From D:\MealKit-Audio [-Ref v0.1.0]
# - -Ref(태그·커밋, 기본 HEAD)의 내용을 git archive로 꺼내므로 체크아웃의 고치는 중인 파일은 들어오지 않는다
# - 가져오는 것: CMakeLists.txt, include/, src/, LICENSE (시험·CI·저장소의 miniaudio·stb는 빼고 엔진의 것을 쓴다)
# - third_party/mkaudio/mealkit.txt의 version을 "<태그 또는 CMake 버전> (<커밋>)"으로 바꾸고 tools\gen_notices.ps1을 돌린다
# 런타임과 엔진을 같이 고치는 동안에는 가져오지 않고 CMake 변수 MEALKIT_MKAUDIO_DIR로 체크아웃을 가리킨다:
#   cmake ... -DMEALKIT_MKAUDIO_DIR=D:/MealKit-Engine/MealKit-Audio   (끝나면 런타임 저장소에 태그를 달고 이 스크립트로 가져온다)
param(
    [Parameter(Mandatory = $true)][string]$From,
    [string]$Ref = "HEAD"
)
$ErrorActionPreference = "Stop"
$engine = Split-Path -Parent $PSScriptRoot
$From = (Resolve-Path $From).Path
$target = Join-Path $engine "third_party\mkaudio"
$utf8 = New-Object System.Text.UTF8Encoding $false

$commit = (& git -C $From rev-parse --short "$Ref^{commit}").Trim()
if ($LASTEXITCODE -ne 0) { throw "not a git commit in ${From}: $Ref" }
$tag = (& git -C $From describe --tags --exact-match $commit 2>$null)
if ($LASTEXITCODE -ne 0) { $tag = "" }
$cmake = (& git -C $From show "${commit}:CMakeLists.txt") -join "`n"
if ($cmake -notmatch 'project\(mkaudio VERSION ([0-9.]+)') { throw "CMakeLists.txt has no project(mkaudio VERSION ...)" }
$version = if ($tag) { $tag.Trim() } else { "v$($Matches[1])+$commit" }

# 꺼내기 (임시 폴더 → 대상 폴더를 mealkit.txt만 남기고 바꾼다)
$tmp = Join-Path ([IO.Path]::GetTempPath()) ("mkaudio-" + [guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory $tmp | Out-Null
try {
    $tar = Join-Path $tmp "src.tar"
    & git -C $From archive --format=tar -o $tar $commit CMakeLists.txt include src LICENSE
    if ($LASTEXITCODE -ne 0) { throw "git archive failed" }
    & (Join-Path $env:SystemRoot "System32\tar.exe") -xf $tar -C $tmp # Windows tar (Git의 tar는 C: 경로를 호스트로 읽는다)
    if ($LASTEXITCODE -ne 0) { throw "tar failed" }
    Remove-Item $tar
    New-Item -ItemType Directory -Force $target | Out-Null
    Get-ChildItem $target | Where-Object { $_.Name -ne "mealkit.txt" } | Remove-Item -Recurse -Force
    Get-ChildItem $tmp | Copy-Item -Destination $target -Recurse -Force
} finally {
    Remove-Item -Recurse -Force $tmp -ErrorAction SilentlyContinue
}

$meta = Join-Path $target "mealkit.txt"
$text = [IO.File]::ReadAllText($meta, $utf8)
$text = [regex]::Replace($text, '(?m)^version .*$', "version $version ($commit)")
[IO.File]::WriteAllText($meta, $text, $utf8)
Write-Host "third_party/mkaudio <- $From $version ($commit)"
& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot "gen_notices.ps1")
exit $LASTEXITCODE

# 도구 제품군 런타임 경계 검사 (docs/DECISIONS.md 2026-10-26 오디오 런타임). check.ps1이 부른다.
#   powershell -ExecutionPolicy Bypass -File tools\check_libs.ps1 [-Path <폴더>]     문제가 있으면 종료 코드 1
# 대상: third_party/<lib>/ 중 mealkit.txt의 source가 MealKit-Engine 저장소인 것(우리가 만든 런타임 조각, 지금 mkaudio),
# 그리고 -Path로 준 폴더(같이 고치는 중인 로컬 체크아웃, MEALKIT_MKAUDIO_DIR). 이 코드는 엔진 밖 저장소에 있으므로:
# - MealKit 헤더(#include "mealkit/..." / <mealkit/...>)와 engine/src 경로를 include하지 않는다
# - MEALKIT_ 매크로(로그·프로파일·단언)를 쓰지 않는다
# - 자기 폴더 밖을 상대 경로("../")로 include하지 않는다
param([string[]]$Path = @())
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot

$dirs = @()
foreach ($d in Get-ChildItem -Path (Join-Path $root "third_party") -Directory) {
    $meta = Join-Path $d.FullName "mealkit.txt"
    if ((Test-Path $meta) -and (Select-String -Path $meta -Pattern '^source https://github\.com/MealKit-Engine/' -Quiet)) { $dirs += $d }
}
foreach ($p in $Path) { $dirs += Get-Item $p }

$problems = @()
$count = 0
foreach ($lib in $dirs) {
    $count++
    foreach ($f in Get-ChildItem -Path $lib.FullName -Recurse -Include *.h, *.hpp, *.c, *.cpp, *.inl, CMakeLists.txt) {
        $rel = $f.FullName.Substring($root.Length + 1)
        $n = 0
        foreach ($line in [IO.File]::ReadAllLines($f.FullName)) {
            $n++
            $code = ($line -replace '//.*$', '') -replace '#\s.*$', '' # 주석 (CMake 주석은 "# " 다음)
            if ($f.Name -eq "CMakeLists.txt") {
                if ($code -match 'MEALKIT_|engine/src|mealkit_') { $problems += "${rel}:${n}: uses the engine build ($($Matches[0]))" }
                continue
            }
            $code = ($line -replace '//.*$', '')
            if ($code -match '^\s*#\s*include\s*["<]([^">]+)[">]') {
                $inc = $Matches[1]
                if ($inc -match '^mealkit/' -or $inc -match 'engine/src') { $problems += "${rel}:${n}: includes $inc (engine header)" }
                elseif ($inc -match '^\.\./\.\./|^\.\./(?!src/|include/)') { $problems += "${rel}:${n}: includes $inc (outside the library)" }
            }
            if ($code -match '\bMEALKIT_\w+') { $problems += "${rel}:${n}: uses $($Matches[0]) (engine macro)" }
        }
    }
}

if ($problems.Count -gt 0) {
    $problems | ForEach-Object { Write-Output "error: $_" }
    Write-Output "FAIL libs: $($problems.Count) problem(s) - tool runtimes must not depend on the engine (docs/DECISIONS.md 2026-10-26)"
    exit 1
}
Write-Output "OK libs: $count library folder(s) do not use engine headers or macros"
exit 0

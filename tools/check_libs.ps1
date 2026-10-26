# 엔진 밖 라이브러리 경계 검사 (docs/DECISIONS.md 2026-10-26 오디오 런타임). check.ps1이 부른다.
#   powershell -ExecutionPolicy Bypass -File tools\check_libs.ps1     문제가 있으면 종료 코드 1
# engine/libs/<lib>/는 별도 저장소로 옮길 라이브러리다(지금 mkaudio). 엔진에 기대면 옮길 수 없으므로:
# - MealKit 헤더(#include "mealkit/..." / <mealkit/...>)와 engine/src 경로를 include하지 않는다
# - MEALKIT_ 매크로(로그·프로파일·단언)를 쓰지 않는다
# - 자기 폴더 밖을 상대 경로("../")로 include하지 않는다
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$libs = Join-Path $root "engine\libs"

$problems = @()
$count = 0
foreach ($lib in Get-ChildItem -Path $libs -Directory) {
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
    Write-Output "FAIL libs: $($problems.Count) problem(s) - engine/libs/ must not depend on the engine (docs/DECISIONS.md 2026-10-26)"
    exit 1
}
Write-Output "OK libs: $count library folder(s) do not use engine headers or macros"
exit 0

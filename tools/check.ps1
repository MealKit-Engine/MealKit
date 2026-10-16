# MealKit 엔진 검증: 엔진을 고친 뒤 한 번에 돌린다. 항목별 PASS/FAIL과 걸린 시간을 출력하고, 하나라도 실패하면 종료 코드 1.
#   powershell -ExecutionPolicy Bypass -File tools\check.ps1          전체
#   powershell -ExecutionPolicy Bypass -File tools\check.ps1 -Quick   빌드 + 단위 테스트 + 결정론 테스트만
# 로그와 스크린샷: build\check\  (항목마다 <번호>-<이름>.log)
# 실행 중인 에디터는 먼저 닫는다 (mealkit.dll을 다시 빌드한다).
param(
    [switch]$Quick
)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$out = Join-Path $root "build\check"
New-Item -ItemType Directory -Force $out | Out-Null
Get-ChildItem $out -File | Remove-Item -Force

$release = Join-Path $root "build\Release\bin"
$editorBin = Join-Path $root "build\Editor\bin"
$game = Join-Path $release "sandbox.exe"
$editor = Join-Path $editorBin "MealKitEditor.exe"
$results = @()
$step = 0

# 항목 하나: $body가 $true를 돌려주면 PASS. 출력은 로그 파일로.
function Invoke-Check([string]$name, [scriptblock]$body) {
    $script:step++
    $log = Join-Path $out ("{0:D2}-{1}.log" -f $script:step, ($name -replace '[^A-Za-z0-9]+', '-'))
    $watch = [Diagnostics.Stopwatch]::StartNew()
    $ok = $false
    $detail = ""
    try {
        $r = & $body $log
        if ($r -is [array]) { $r = $r[-1] }
        if ($r -is [string]) { $detail = $r; $ok = $false }
        else { $ok = [bool]$r }
    } catch {
        $detail = $_.Exception.Message
        Add-Content -Path $log -Value $detail
    }
    $watch.Stop()
    $status = if ($ok) { "PASS" } else { "FAIL" }
    $color = if ($ok) { "Green" } else { "Red" }
    Write-Host ("{0,-4} {1,-34} {2,7:N1}s  {3}" -f $status, $name, $watch.Elapsed.TotalSeconds, $detail) -ForegroundColor $color
    $script:results += [pscustomobject]@{ Name = $name; Ok = $ok; Seconds = $watch.Elapsed.TotalSeconds; Log = $log }
}

# 프로그램을 실행하고 stdout/stderr를 로그에 남긴다 (cmd가 직접 파일로 보낸다). 종료 코드를 돌려준다.
function Run([string]$log, [string]$exe, [string]$arguments) {
    & cmd.exe /d /s /c "`"`"$exe`" $arguments > `"$log`" 2>&1`""
    return $LASTEXITCODE
}

function Build([string]$log, [string]$kind) {
    $code = Run $log "$root\build.bat" $kind
    return $code -eq 0 -and (Select-String -Path $log -Pattern "Build OK|Export OK" -Quiet)
}

function Has([string]$log, [string]$pattern) { return [bool](Select-String -Path $log -Pattern $pattern -Quiet) }

Write-Host "MealKit check ($(if ($Quick) { 'quick' } else { 'full' })) - logs in $out"

Invoke-Check "Editor build" { param($log) Build $log "Editor" }
Invoke-Check "Release build" { param($log) Build $log "" }
Invoke-Check "Unit tests (Release)" { param($log) (Run $log "$release\mealkit_tests.exe" "") -eq 0 }
Invoke-Check "Unit tests (Editor) + DetMath bits" {
    param($log)
    if ((Run $log "$editorBin\mealkit_tests.exe" "") -ne 0) { return $false }
    $a = (Select-String -Path $log -Pattern "^detmath-bits (\w+)").Matches[0].Groups[1].Value
    $b = (Select-String -Path $results[2].Log -Pattern "^detmath-bits (\w+)").Matches[0].Groups[1].Value
    if ($a -ne $b) { return "Release $b != Editor $a" }
    return $true
}
Invoke-Check "Editor strings (ko.txt)" {
    param($log)
    $output = & powershell -NoProfile -ExecutionPolicy Bypass -File "$PSScriptRoot\check_strings.ps1" 2>&1
    $output | Out-File -Encoding utf8 $log
    return $LASTEXITCODE -eq 0
}
Invoke-Check "Determinism (mp_test 1+3, 900 ticks)" {
    param($log)
    $output = & powershell -NoProfile -ExecutionPolicy Bypass -File "$PSScriptRoot\mp_test.ps1" -Clients 3 -Ticks 900 2>&1
    $output | Out-File -Encoding utf8 $log
    return ($output -match "^PASS").Count -gt 0
}

if (-not $Quick) {
    Invoke-Check "Shadow simulation + rewind" {
        param($log)
        Run $log $game "--bot --seed 7 --exit-tick 900 --shadow --rewind-test --vsync 0 --size 640 360" | Out-Null
        return (Has $log "^SHADOW ok") -and (Has $log "^REWIND ok")
    }
    Invoke-Check "Game capture" {
        param($log)
        $png = Join-Path $out "game.png"
        $code = Run $log $game "--bot --seed 7 --exit-tick 300 --capture `"$png`" 240"
        return $code -eq 0 -and (Test-Path $png)
    }
    Invoke-Check "Editor capture (edit mode)" {
        param($log)
        $png = Join-Path $out "editor.png"
        $code = Run $log $editor "--project `"$root`" --frames 60 --select 25 --capture `"$png`""
        return $code -eq 0 -and (Test-Path $png) -and -not (Has $log "\[ERROR\]")
    }
    Invoke-Check "Editor play (solo, shadow)" {
        param($log)
        $png = Join-Path $out "editor-play.png"
        $code = Run $log $editor "--project `"$root`" --frames 180 --play --bot --capture `"$png`""
        return $code -eq 0 -and (Test-Path $png) -and (Has $log "Play mode") -and -not (Has $log "Shadow simulation diverged")
    }
    Invoke-Check "Editor play (3 players)" {
        param($log)
        $png = Join-Path $out "editor-3p.png"
        $code = Run $log $editor "--project `"$root`" --frames 400 --play --players 3 --bot --capture `"$png`""
        return $code -eq 0 -and (Has $log "Play mode: \d+ bodies, 3 player") -and -not (Has $log "Play stopped")
    }
    # 빌드 실패: sim/에 금지 코드를 넣으면 SimCheck가 빌드를 멈추고, 에디터는 이전 DLL로 계속 플레이할 수 있어야 한다
    $bad = Join-Path $root "samples\sandbox\sim\ZzCheckFailure.cpp"
    try {
        Set-Content -Path $bad -Value "// tools/check.ps1이 잠깐 만드는 파일 (SimCheck 실패 시험)`r`n#include <random>`r`n" -Encoding UTF8
        Invoke-Check "SimCheck stops a bad build" {
            param($log)
            $ok = -not (Build $log "Editor")
            return $ok -and (Has $log "SIM002")
        }
        Invoke-Check "Editor keeps the old DLL on failure" {
            param($log)
            Run $log $editor "--project `"$root`" --build-game --frames 30" | Out-Null
            return (Has $log "Build failed") -and (Has $log "Game module '.*' loaded")
        }
    } finally {
        Remove-Item $bad -Force -ErrorAction SilentlyContinue
    }
    Invoke-Check "Game code rebuild + load" {
        param($log)
        Run $log $editor "--project `"$root`" --build-game --frames 30" | Out-Null
        return (Has $log "Game code build finished") -and (Has $log "Game module '.*' loaded") -and -not (Has $log "Build failed")
    }
    Invoke-Check "New project + upgrade" {
        param($log)
        $projects = Join-Path $env:TEMP "mealkit-check-projects" # 엔진 폴더 밖이어야 한다
        Remove-Item -Recurse -Force $projects -ErrorAction SilentlyContinue
        $output = & powershell -NoProfile -ExecutionPolicy Bypass -File "$PSScriptRoot\new_project.ps1" -Name CheckGame -Path $projects 2>&1
        $output += & powershell -NoProfile -ExecutionPolicy Bypass -File "$PSScriptRoot\upgrade_project.ps1" -Project "$projects\CheckGame" 2>&1
        $output | Out-File -Encoding utf8 $log
        $ok = (Test-Path "$projects\CheckGame\.mealkit-template") -and (($output -match "^Done: 0 change").Count -gt 0)
        Remove-Item -Recurse -Force $projects -ErrorAction SilentlyContinue
        return $ok
    }
    Invoke-Check "Export build" {
        param($log)
        return (Build $log "Export") -and (Test-Path (Join-Path $root "export\sandbox\sandbox.exe"))
    }
}

$failed = @($results | Where-Object { -not $_.Ok })
$total = ($results | Measure-Object -Property Seconds -Sum).Sum
Write-Host ("{0} passed, {1} failed, {2:N0}s total" -f ($results.Count - $failed.Count), $failed.Count, $total) `
    -ForegroundColor $(if ($failed.Count -eq 0) { "Green" } else { "Red" })
if ($failed.Count -gt 0) { exit 1 }
exit 0

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
$headless = Join-Path $release "sandbox_headless.exe"
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
        if ($r -is [string]) { $detail = $r; $ok = $r.StartsWith("SKIP") } # "SKIP: 이유" = 이 PC에서 할 수 없음 (실패 아님)
        else { $ok = [bool]$r }
    } catch {
        $detail = $_.Exception.Message
        Add-Content -Path $log -Value $detail
    }
    $watch.Stop()
    $status = if ($detail.StartsWith("SKIP")) { "SKIP" } elseif ($ok) { "PASS" } else { "FAIL" }
    $color = if ($status -eq "SKIP") { "Yellow" } elseif ($ok) { "Green" } else { "Red" }
    Write-Host ("{0,-4} {1,-34} {2,7:N1}s  {3}" -f $status, $name, $watch.Elapsed.TotalSeconds, $detail) -ForegroundColor $color
    $script:results += [pscustomobject]@{ Name = $name; Ok = $ok; Skipped = ($status -eq "SKIP"); Seconds = $watch.Elapsed.TotalSeconds; Log = $log }
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
Invoke-Check "Project layout + notices" {
    param($log)
    $output = & powershell -NoProfile -ExecutionPolicy Bypass -File "$PSScriptRoot\check_layout.ps1" 2>&1
    $code = $LASTEXITCODE
    $output += & powershell -NoProfile -ExecutionPolicy Bypass -File "$PSScriptRoot\gen_notices.ps1" -Check 2>&1
    $output | Out-File -Encoding utf8 $log
    return $code -eq 0 -and $LASTEXITCODE -eq 0
}
Invoke-Check "Platform layer (OS APIs)" {
    param($log)
    $output = & powershell -NoProfile -ExecutionPolicy Bypass -File "$PSScriptRoot\check_platform.ps1" 2>&1
    $output | Out-File -Encoding utf8 $log
    return $LASTEXITCODE -eq 0
}
Invoke-Check "Engine layers (include order)" {
    param($log)
    $output = & powershell -NoProfile -ExecutionPolicy Bypass -File "$PSScriptRoot\check_layers.ps1" 2>&1
    $output | Out-File -Encoding utf8 $log
    return $LASTEXITCODE -eq 0
}
Invoke-Check "Determinism (mp_test 1+3, 900 ticks)" {
    param($log)
    $output = & powershell -NoProfile -ExecutionPolicy Bypass -File "$PSScriptRoot\mp_test.ps1" -Clients 3 -Ticks 900 2>&1
    $output | Out-File -Encoding utf8 $log
    return ($output -match "^PASS").Count -gt 0
}

# 헤드리스: 창 없이 같은 결정론 테스트 + 입력 기록을 다른 실행 파일(sandbox.exe --headless)로 재생해 틱별 해시가 같은지
Invoke-Check "Headless (mp_test, record + replay)" {
    param($log)
    $output = & powershell -NoProfile -ExecutionPolicy Bypass -File "$PSScriptRoot\mp_test.ps1" -Clients 3 -Ticks 900 -Headless 2>&1
    $output | Out-File -Encoding utf8 $log
    if (($output -match "^PASS").Count -eq 0) { return "mp_test -Headless failed" }
    $rec = Join-Path $out "headless.rec"
    Run "$log.record" $headless "--seed 7 --exit-tick 900 --record-inputs `"$rec`" --hash-log `"$out\headless-hashes.txt`"" | Out-Null
    $code = Run "$log.replay" $game "--headless --replay-inputs `"$rec`" --hash-log `"$out\replay-hashes.txt`""
    if ($code -ne 0 -or -not (Has "$log.replay" "^REPLAY ok")) { return "replay failed" }
    if ((Get-FileHash "$out\headless-hashes.txt").Hash -ne (Get-FileHash "$out\replay-hashes.txt").Hash) { return "hash logs differ" }
    return $true
}

# 저장소의 고정 입력 기록(samples/*/replays/*.rec, CI가 모든 OS에서 재생한다)이 지금 코드와 맞는가.
# 시뮬레이션 결과를 일부러 바꿨다면 기록을 다시 만든다 (문서 저장소 "엔진 개발 > 테스트").
Invoke-Check "Committed replays (samples/*/replays)" {
    param($log)
    $recs = @(Get-ChildItem -Path (Join-Path $root "samples") -Recurse -Filter *.rec | Where-Object { $_.Directory.Name -eq "replays" })
    if ($recs.Count -eq 0) { return "no recordings" }
    foreach ($rec in $recs) {
        $exe = Join-Path $release "$($rec.Directory.Parent.Name)_headless.exe"
        $code = Run "$log.$($rec.BaseName)" $exe "--replay-inputs `"$($rec.FullName)`""
        if ($code -ne 0 -or -not (Has "$log.$($rec.BaseName)" "^REPLAY ok")) { return "$($rec.Name) differs - re-record if intended" }
    }
    return $true
}

if (-not $Quick) {
    Invoke-Check "Shadow simulation + rewind" {
        param($log)
        Run $log $game "--bot --mute --seed 7 --exit-tick 900 --shadow --rewind-test --vsync 0 --size 640 360 --hash-log `"$out\window-hashes.txt`"" | Out-Null
        return (Has $log "^SHADOW ok") -and (Has $log "^REWIND ok")
    }
    Invoke-Check "Headless = window (900 tick hashes)" {
        param($log)
        $a = Join-Path $out "headless-hashes.txt"
        $b = Join-Path $out "window-hashes.txt"
        if (-not (Test-Path $a) -or -not (Test-Path $b)) { return "hash log missing" }
        return (Get-FileHash $a).Hash -eq (Get-FileHash $b).Hash
    }
    Invoke-Check "Game capture" {
        param($log)
        $png = Join-Path $out "game.png"
        $code = Run $log $game "--bot --mute --seed 7 --exit-tick 300 --capture `"$png`" 240"
        return $code -eq 0 -and (Test-Path $png)
    }
    # 화면 흐름: 타이틀 → 메뉴 → 혼자 하기 → 로비 → 시작 → 매치 (위젯을 이름으로 누른다, 프레임 수로 끝)
    Invoke-Check "Game screen flow (title to match)" {
        param($log)
        $png = Join-Path $out "flow.png"
        $code = Run $log $game "--mute --frames 240 --ui-press 20:start,50:solo,90:start --capture `"$png`""
        return $code -eq 0 -and (Test-Path $png) -and (Has $log "Match 1 started") -and (Has $log "World ready") -and -not (Has $log "--ui-press: no widget")
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
    # RHI 백엔드 (ROADMAP 2단계 "RHI 층"): Vulkan 에디터가 같은 장면을 DX12와 같게 그리는가, 검증 레이어 오류가 없는가.
    # 에디터 편집 화면은 실행마다 같다(게임 화면은 시간에 따라 바뀐다). 상태 표시줄(fps)은 뺀다.
    $vkEditorBin = Join-Path $root "build\VulkanEditor\bin"
    $vkEditor = Join-Path $vkEditorBin "MealKitEditor.exe"
    Invoke-Check "Vulkan editor build" { param($log) Build $log "VulkanEditor" }
    Invoke-Check "Vulkan = DX12 (editor capture)" {
        param($log)
        # 창 배치가 같아야 한다: DX12 에디터의 창 배치 파일을 쓴다
        $ini = Join-Path $editorBin "mealkit_editor.ini"
        if (Test-Path $ini) { Copy-Item $ini $vkEditorBin -Force }
        $dx = Join-Path $out "backend-dx12.png"
        $vk = Join-Path $out "backend-vulkan.png"
        $common = "--project `"$root`" --frames 60 --select 25 --capture"
        if ((Run "$log.dx12" $editor "$common `"$dx`"") -ne 0 -or -not (Test-Path $dx)) { return "DX12 capture failed" }
        if ((Run "$log.vulkan" $vkEditor "$common `"$vk`"") -ne 0 -or -not (Test-Path $vk)) { return "Vulkan capture failed" }
        $output = & powershell -NoProfile -ExecutionPolicy Bypass -File "$PSScriptRoot\compare_images.ps1" -A $dx -B $vk -IgnoreBottom 40 2>&1
        $output | Out-File -Encoding utf8 $log
        if ($LASTEXITCODE -ne 0) { return ($output | Select-Object -First 1) }
        return $true
    }
    Invoke-Check "Vulkan validation layer (play)" {
        param($log)
        $png = Join-Path $out "vulkan-play.png"
        $code = Run $log $vkEditor "--project `"$root`" --frames 180 --play --bot --debug --capture `"$png`""
        if (Has $log "validation layer unavailable") { return "SKIP: Vulkan SDK not installed (validation layer)" }
        if ($code -ne 0 -or -not (Has $log "Play mode")) { return "play failed" }
        $issues = @(Select-String -Path $log -Pattern "\[Vulkan\]")
        if ($issues.Count -gt 0) { return "$($issues.Count) validation message(s)" }
        return $true
    }
    Invoke-Check "Editor play (3 players)" {
        param($log)
        $png = Join-Path $out "editor-3p.png"
        $code = Run $log $editor "--project `"$root`" --frames 400 --play --players 3 --bot --capture `"$png`""
        return $code -eq 0 -and (Has $log "Play mode: 3 player") -and -not (Has $log "Play stopped")
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
        $output += & powershell -NoProfile -ExecutionPolicy Bypass -File "$PSScriptRoot\check_layout.ps1" -Project "$projects\CheckGame" 2>&1
        $layoutOk = $LASTEXITCODE -eq 0 -and (($output -match "^OK layout: .* \(0 warning").Count -gt 0)
        # 게임 라이브러리: 헤더 전용 라이브러리를 넣으면 view/에서는 되고 sim/에서는 SimCheck SIM011
        $lib = "$projects\CheckGame\third_party\checklib"
        New-Item -ItemType Directory -Force $lib | Out-Null
        Set-Content "$lib\checklib.h" "#pragma once`r`ninline int checkLib() { return 1; }" -Encoding UTF8
        Set-Content "$lib\LICENSE" "MIT" -Encoding UTF8
        Set-Content "$lib\mealkit.txt" "name checklib`r`nversion 1`r`nlicense MIT`r`nsource local`r`n" -Encoding UTF8
        Set-Content "$projects\CheckGame\source\sim\ZzLib.cpp" "#include `"checklib.h`"`r`n" -Encoding UTF8
        $sim = & "$root\build\Release\tools\mealkit_simcheck.exe" "$projects\stamp" --third-party $lib "$projects\CheckGame\source\sim\ZzLib.cpp" 2>&1
        $output += $sim
        $simOk = ($sim -match "SIM011").Count -gt 0
        $output | Out-File -Encoding utf8 $log
        $ok = (Test-Path "$projects\CheckGame\.mealkit-template") -and (($output -match "^Done: 0 change").Count -gt 0) -and $layoutOk -and $simOk
        Remove-Item -Recurse -Force $projects -ErrorAction SilentlyContinue
        return $ok
    }
    Invoke-Check "Export build" {
        param($log)
        return (Build $log "Export") -and (Test-Path (Join-Path $root "export\sandbox\sandbox.exe"))
    }
}

$failed = @($results | Where-Object { -not $_.Ok })
$skipped = @($results | Where-Object { $_.Skipped })
$total = ($results | Measure-Object -Property Seconds -Sum).Sum
Write-Host ("{0} passed, {1} skipped, {2} failed, {3:N0}s total" -f ($results.Count - $failed.Count - $skipped.Count), $skipped.Count, $failed.Count, $total) `
    -ForegroundColor $(if ($failed.Count -eq 0) { "Green" } else { "Red" })
if ($failed.Count -gt 0) { exit 1 }
exit 0

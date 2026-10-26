# MealKit 엔진 검증: 엔진을 고친 뒤 한 번에 돌린다. 항목별 PASS/FAIL과 걸린 시간을 출력하고, 하나라도 실패하면 종료 코드 1.
#   powershell -ExecutionPolicy Bypass -File tools\check.ps1          전체
#   powershell -ExecutionPolicy Bypass -File tools\check.ps1 -Quick   빌드 + 단위 테스트 + 결정론 테스트만
#   ... -UpdateGolden   그래픽 회귀 테스트의 기준 그림(engine\tests\golden)을 이번 그림으로 바꾼다 (그림을 일부러 바꿨을 때)
# 로그와 스크린샷: build\check\  (항목마다 <번호>-<이름>.log)
# 실행 중인 에디터는 먼저 닫는다 (mealkit.dll을 다시 빌드한다).
param(
    [switch]$Quick,
    [switch]$UpdateGolden
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
# 도구 제품군 런타임(third_party/mkaudio = 별도 저장소 MealKit-Audio)은 엔진 헤더·매크로를 쓰지 않는다
Invoke-Check "Engine libs (no engine headers)" {
    param($log)
    $output = & powershell -NoProfile -ExecutionPolicy Bypass -File "$PSScriptRoot\check_libs.ps1" 2>&1
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
    # GPU 캡처 (ROADMAP "GPU 캡처 버튼과 에디터 보기 모드"): RenderDoc이 있으면 --gpu-capture-at으로 한 프레임을 캡처해 .rdc가 생기는지
    Invoke-Check "GPU capture (RenderDoc)" {
        param($log)
        if (-not (Test-Path "$env:ProgramFiles\RenderDoc\renderdoc.dll")) { return "SKIP: RenderDoc not installed" }
        $dir = Join-Path $env:LOCALAPPDATA "MealKit\gpu-captures"
        $before = @(Get-ChildItem $dir -Filter *.rdc -ErrorAction SilentlyContinue).Count
        $code = Run $log $game "--bot --mute --vsync 0 --exit-tick 120 --gpu-capture-at 30"
        $after = @(Get-ChildItem $dir -Filter *.rdc -ErrorAction SilentlyContinue)
        if ($after.Count -gt $before) { Remove-Item ($after | Sort-Object LastWriteTime | Select-Object -Last 1).FullName } # 시험 캡처는 지운다
        return $code -eq 0 -and $after.Count -gt $before -and (Has $log "RenderDoc loaded")
    }
    Invoke-Check "Game screen flow (title to match)" {
        param($log)
        $png = Join-Path $out "flow.png"
        $code = Run $log $game "--mute --frames 240 --ui-press 20:start,50:solo,90:start --capture `"$png`""
        return $code -eq 0 -and (Test-Path $png) -and (Has $log "Match 1 started") -and (Has $log "World ready") -and -not (Has $log "--ui-press: no widget")
    }
    # 라이브 연결 (도구 사이 약속, mealkit/live/Server.h): 게임의 build\live\ 접속 정보로 hello + stats 이벤트, 끝나면 접속 정보 파일이 없어진다
    Invoke-Check "Live connection (game)" {
        param($log)
        $p = Start-Process -FilePath $game -ArgumentList "--bot --mute --vsync 0 --size 640 360 --exit-tick 600" -PassThru -WindowStyle Hidden
        try {
            & powershell -NoProfile -ExecutionPolicy Bypass -File "$root\tools\live_probe.ps1" -Kind game -Wait 20 -Events 2 *> $log
            $code = $LASTEXITCODE
        } finally { $p.WaitForExit(60000) | Out-Null }
        $left = Test-Path (Join-Path $root "build\live\game-$($p.Id).json")
        return $code -eq 0 -and -not $left -and (Has $log "^event stats: ")
    }
    # 도구 바로가기 (editor/Tools.h): 매니페스트를 찾고, 약속 버전이 다른 도구를 알아본다
    Invoke-Check "Editor tool discovery (manifest)" {
        param($log)
        $dir = Join-Path $out "tools"
        Remove-Item -Recurse -Force $dir -ErrorAction SilentlyContinue
        New-Item -ItemType Directory -Force "$dir\good", "$dir\old" | Out-Null
        [IO.File]::WriteAllText("$dir\good\mealkit-tool.txt", "# MealKit tool manifest`n@format tool 1`nname Check Tool`nexe $game`nversion 1.0`nprotocol 1`nopens .table`n")
        [IO.File]::WriteAllText("$dir\old\mealkit-tool.txt", "@format tool 1`nname Old Tool`nexe old.exe`nprotocol 999`n")
        $code = Run $log $editor "--list-tools --tool-path `"$dir`""
        return $code -eq 0 -and (Has $log "^tool: Check Tool 1\.0 protocol 1 .*sandbox\.exe$") -and (Has $log "^tool: Old Tool .*\(protocol differs\)")
    }
    # Penpot 플러그인 (ROADMAP "게임 화면 UI 2부"): 변환기 시험(Node.js가 있을 때), Penpot에서 내보낸 화면의 배치 = Penpot 상자.
    # 비교는 영어 표로 한다 (Penpot 원본 글자가 영어라 글자 노드도 같은 글자를 그린다)
    Invoke-Check "Penpot plugin (converter tests)" {
        param($log)
        if (-not (Get-Command node -ErrorAction SilentlyContinue)) { return "SKIP: no Node.js" }
        $code = Run $log "node" "--test `"$root\tools\penpot-plugin\test\convert.test.ts`""
        return $code -eq 0
    }
    # Blender 애드온 (ROADMAP "Blender 내보내기 애드온"): Blender LTS를 창 없이 돌려 검사·내보내기 시험 (없으면 SKIP).
    # 지원 범위 스키마가 엔진과 같은지는 단위 테스트 asset_schema가 본다
    Invoke-Check "Blender add-on export" {
        param($log)
        $blender = $env:MEALKIT_BLENDER
        if (-not $blender) { $blender = "$env:ProgramFiles\Blender Foundation\Blender 5.2\blender.exe" }
        if (-not (Test-Path $blender)) { return "SKIP: no Blender 5.2 (set MEALKIT_BLENDER)" }
        $dir = Join-Path $out "blender-addon"
        if (Test-Path $dir) { Remove-Item $dir -Recurse -Force }
        # Run(cmd /s /c)은 공백 있는 exe 경로(Program Files)를 감싸지 못해 직접 부른다
        $ErrorActionPreference = "Continue" # Blender가 stderr에 쓰는 줄을 오류로 보지 않는다
        & $blender -b --factory-startup --python "$root\tools\blender-addon\test\test_export.py" -- $dir 2>&1 | Out-File $log -Encoding utf8
        return $LASTEXITCODE -eq 0 -and (Has $log "BLENDER ADDON ok")
    }
    Invoke-Check "Penpot screens = engine layout" {
        param($log)
        $code = Run $log $editor "--project `"$root`" --ui-check --lang en"
        return $code -eq 0 -and (Has $log "UI-CHECK .* -> ok") -and -not (Has $log "-> FAIL")
    }
    # 보조: Penpot이 그린 보드(art/ui/preview, 반 크기)와 엔진이 그린 같은 화면. 글자 래스터·안티에일리어싱이 달라 느슨하게
    # (채널 차이 48 넘는 픽셀 1% 이하). 노드가 빠지거나 크게 어긋나는 것을 잡는다
    Invoke-Check "Penpot screens = engine pixels" {
        param($log)
        $previews = @(Get-ChildItem (Join-Path $root "art\ui\preview") -Filter *.png -ErrorAction SilentlyContinue)
        if ($previews.Count -eq 0) { return "SKIP: no art/ui/preview images" }
        foreach ($p in $previews) {
            $png = Join-Path $out ("engine-" + $p.Name)
            $code = Run "$log.render" $editor "--project `"$root`" --lang en --ui-render ui/$($p.BaseName).ui `"$png`""
            Get-Content "$log.render" | Add-Content $log
            if ($code -ne 0) { return $false }
            $diff = & powershell -NoProfile -ExecutionPolicy Bypass -File "$PSScriptRoot\compare_images.ps1" -A $p.FullName -B $png -Threshold 48 -MaxBadPercent 1 -MaxDiff 255 2>&1
            "$($p.Name): $diff" | Add-Content $log
            if ($LASTEXITCODE -ne 0) { return $false }
        }
        return $true
    }
    Invoke-Check "Editor capture (edit mode)" {
        param($log)
        $png = Join-Path $out "editor.png"
        $code = Run $log $editor "--project `"$root`" --frames 60 --select 25 --capture `"$png`""
        return $code -eq 0 -and (Test-Path $png) -and -not (Has $log "\[ERROR\]")
    }
    # 그래픽 회귀 테스트 (ROADMAP "그래픽 회귀 테스트"): 고정 장면(engine\tests\golden\shots.txt)을 UI 없이 그려 기준 그림과 비교.
    # 다르면 build\check\golden\<이름>-diff.png (빨강 = 다른 픽셀). GPU·드라이버가 다르면 조금 다를 수 있어 문턱을 둔다
    $goldenDir = Join-Path $root "engine\tests\golden"
    function Compare-Golden([string]$log, [string]$exe, [string]$suffix) {
        $dir = Join-Path $out "golden$suffix"
        $code = Run $log $exe "--project `"$root`" --render-golden `"$goldenDir\shots.txt`" `"$dir`""
        if ($code -ne 0) { return "render failed" }
        $problems = @()
        foreach ($png in Get-ChildItem $dir -Filter *.png | Where-Object { $_.Name -notlike "*-diff.png" }) {
            $golden = Join-Path $goldenDir $png.Name
            if ($UpdateGolden -and $suffix -eq "") { Copy-Item $png.FullName $golden -Force; continue }
            if (-not (Test-Path $golden)) { $problems += "$($png.BaseName): no golden image (run check.ps1 -UpdateGolden)"; continue }
            $diff = Join-Path $dir ($png.BaseName + "-diff.png")
            $r = & powershell -NoProfile -ExecutionPolicy Bypass -File "$PSScriptRoot\compare_images.ps1" -A $golden -B $png.FullName -Threshold 8 -MaxBadPercent 0.5 -MaxDiff 255 -DiffOut $diff 2>&1
            Add-Content $log "$($png.Name): $($r | Select-Object -First 1)"
            if ($LASTEXITCODE -ne 0) { $problems += "$($png.BaseName): $($r | Select-Object -First 1)" }
        }
        if ($UpdateGolden -and $suffix -eq "") { return "SKIP: golden images updated in engine\tests\golden" }
        if ($problems.Count -gt 0) { return ($problems -join "; ") }
        return $true
    }
    Invoke-Check "Graphics regression (golden images)" { param($log) Compare-Golden $log $editor "" }
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
        $output = & powershell -NoProfile -ExecutionPolicy Bypass -File "$PSScriptRoot\compare_images.ps1" -A $dx -B $vk -IgnoreBottom 40 -DiffOut (Join-Path $out "backend-diff.png") 2>&1
        $output | Out-File -Encoding utf8 $log
        if ($LASTEXITCODE -ne 0) { return ($output | Select-Object -First 1) }
        return $true
    }
    Invoke-Check "Graphics regression (Vulkan)" { param($log) Compare-Golden $log $vkEditor "-vulkan" }
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
        $exe = Join-Path $root "export\sandbox\sandbox.exe"
        if (-not ((Build $log "Export") -and (Test-Path $exe))) { return $false }
        # 출시 빌드에는 라이브 연결 서버가 없다 (MEALKIT_LIVE=0)
        $text = [Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($exe))
        if ($text.Contains("Live server (")) { return "export\sandbox\sandbox.exe contains the live server (MEALKIT_LIVE should be 0)" }
        return $true
    }
}

$failed = @($results | Where-Object { -not $_.Ok })
$skipped = @($results | Where-Object { $_.Skipped })
$total = ($results | Measure-Object -Property Seconds -Sum).Sum
Write-Host ("{0} passed, {1} skipped, {2} failed, {3:N0}s total" -f ($results.Count - $failed.Count - $skipped.Count), $skipped.Count, $failed.Count, $total) `
    -ForegroundColor $(if ($failed.Count -eq 0) { "Green" } else { "Red" })
if ($failed.Count -gt 0) { exit 1 }
exit 0

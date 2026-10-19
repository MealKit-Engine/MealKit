# 엔진 폴더 계층 검사 (docs/CODING_MODEL.md "엔진 폴더 사이 의존"). check.ps1이 부른다.
#   powershell -ExecutionPolicy Bypass -File tools\check_layers.ps1     문제가 있으면 종료 코드 1
# engine/src/mealkit/<폴더>의 파일은 아래 순서에서 자기보다 앞의(또는 같은 칸의) 폴더만 #include "mealkit/<폴더>/..."로 쓴다.
# 순서에 없는 폴더가 생기면 실패한다: 새 폴더는 여기와 CODING_MODEL.md에 자리를 정하고 넣는다.
# platform/<os>/ 구현 파일은 여러 폴더의 헤더(net/Socket.h, core/Clock.h 등)를 구현하므로 빼고, check_platform.ps1이 본다.
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$engineSrc = Join-Path $root "engine\src\mealkit"

# 앞에서 뒤로. 한 칸에 여러 폴더면 서로 include할 수 있다(기반 core·platform만)
$layers = @(
    @("core", "platform"), # 기반
    @("net"), @("physics"), @("sim"), @("data"), # 시뮬레이션 쪽 (헤드리스가 쓰는 것)
    @("loc"), @("scene"), @("rhi"), @("render"), @("asset"), @("online"), @("debug"), # 화면 쪽
    @("input"), @("audio"), @("ui"), @("feedback"), @("settings"),
    @("game"), # 게임 경계 (GameInstance, GameContext)
    @("editor")
)
$rank = @{}
for ($i = 0; $i -lt $layers.Count; $i++) { foreach ($d in $layers[$i]) { $rank[$d] = $i } }

$problems = @()
foreach ($dir in Get-ChildItem -Path $engineSrc -Directory) {
    if (-not $rank.ContainsKey($dir.Name)) { $problems += "engine\src\mealkit\$($dir.Name): folder is not in the layer order (tools\check_layers.ps1)" }
}

$files = Get-ChildItem -Path $engineSrc -Recurse -Include *.cpp, *.h |
    Where-Object { $_.FullName -notmatch '\\mealkit\\platform\\(win32|posix)\\' }
$edges = 0
foreach ($f in $files) {
    $rel = $f.FullName.Substring($root.Length + 1)
    $from = $f.FullName.Substring($engineSrc.Length + 1).Split('\')[0]
    if (-not $rank.ContainsKey($from)) { continue } # 위에서 이미 알렸다
    $n = 0
    foreach ($line in [IO.File]::ReadAllLines($f.FullName)) {
        $n++
        $code = ($line -replace '//.*$', '')
        if ($code -notmatch '^\s*#\s*include\s*["<]mealkit/(\w+)/') { continue }
        $to = $Matches[1]
        if ($to -eq $from) { continue }
        $edges++
        if (-not $rank.ContainsKey($to)) { $problems += "${rel}:${n}: includes mealkit/$to/ which is not in the layer order"; continue }
        if ($rank[$to] -gt $rank[$from]) { $problems += "${rel}:${n}: $from/ includes $to/ (later layer)" }
    }
}

$order = ($layers | ForEach-Object { $_ -join "+" }) -join " < "
if ($problems.Count -gt 0) {
    $problems | ForEach-Object { Write-Host $_ }
    Write-Host "order: $order"
    Write-Host "FAIL: $($problems.Count) problem(s) with the engine layer order" -ForegroundColor Red
    exit 1
}
Write-Host "OK: $($files.Count) files, $edges cross-folder includes follow $order" -ForegroundColor Green
exit 0

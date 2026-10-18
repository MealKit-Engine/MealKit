# 플랫폼 계층 검사 (docs/CODING_MODEL.md "그래픽 API와 OS 기능"). check.ps1이 부른다.
#   powershell -ExecutionPolicy Bypass -File tools\check_platform.ps1     문제가 있으면 종료 코드 1
# OS API(헤더 include, 대표 Win32·POSIX 함수)는 engine/src/mealkit/platform/<os>/ 안에서만 쓴다.
# 게임 코드(samples/*)는 OS API를 쓰지 않는다 (GameContext로 받는다).
# 예외: RHI 층(2단계)이 생기기 전까지 그래픽 API를 직접 쓰는 파일 ($exceptions). 옮기면 목록에서 지운다.
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$engineSrc = Join-Path $root "engine\src\mealkit"

$exceptions = @{
    "engine\src\mealkit\render\Renderer.cpp"   = "DX12 renderer (moves into mealkit/rhi/ in stage 2)"
    "engine\src\mealkit\debug\ImGuiLayer.cpp"  = "ImGui Win32 + DX12 backends (moves with the RHI in stage 2)"
}

$osHeaders = 'windows\.h|winsock2?\.h|ws2tcpip\.h|mstcpip\.h|shobjidl\.h|shlobj\.h|shellapi\.h|commdlg\.h|wincodec\.h|dwmapi\.h|' +
    'd3d12\.h|dxgi\w*\.h|d3dcompiler\.h|wrl/\w+\.h|unistd\.h|pthread\.h|dlfcn\.h|fcntl\.h|netdb\.h|sys/\w+\.h|arpa/\w+\.h|' +
    'netinet/\w+\.h|mach-o/\w+\.h|xaudio2\.h|xinput\.h'
$includePattern = "^\s*#\s*include\s*<($osHeaders)>"
$callPattern = '\b(MultiByteToWideChar|WideCharToMultiByte|_wfopen(_s)?|LoadLibrary\w*|GetProcAddress|FreeLibrary|GetModuleHandle\w*|' +
    'GetModuleFileName\w*|MessageBox\w*|OutputDebugString\w*|GetEnvironmentVariable\w*|CreateFile\w*|ShellExecute\w*|' +
    'CreateProcess\w*|QueryPerformanceCounter|WSAStartup|dlopen|dlsym|getenv)\s*\('

$files = @()
$files += Get-ChildItem -Path $engineSrc -Recurse -Include *.cpp, *.h |
    Where-Object { $_.FullName -notmatch '\\mealkit\\platform\\(win32|posix)\\' }
$files += Get-ChildItem -Path (Join-Path $root "samples") -Recurse -Include *.cpp, *.h

$problems = @()
foreach ($f in $files) {
    $rel = $f.FullName.Substring($root.Length + 1)
    if ($exceptions.ContainsKey($rel)) { continue }
    $n = 0
    foreach ($line in [IO.File]::ReadAllLines($f.FullName)) {
        $n++
        $code = ($line -replace '//.*$', '')
        if ($code -cmatch $includePattern) { $problems += "${rel}:${n}: OS header <$($Matches[1])> outside platform/<os>/" }
        elseif ($code -cmatch $callPattern) { $problems += "${rel}:${n}: OS call $($Matches[1])() outside platform/<os>/" }
    }
}

foreach ($e in $exceptions.GetEnumerator()) { Write-Host "allowed: $($e.Key) - $($e.Value)" }
if ($problems.Count -gt 0) {
    $problems | ForEach-Object { Write-Host $_ }
    Write-Host "FAIL: $($problems.Count) OS API use(s) outside the platform layer" -ForegroundColor Red
    exit 1
}
Write-Host "OK: $($files.Count) files, OS APIs only in platform/<os>/" -ForegroundColor Green
exit 0

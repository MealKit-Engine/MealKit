# 플랫폼 계층 검사 (docs/CODING_MODEL.md "그래픽 API와 OS 기능"). check.ps1이 부른다.
#   powershell -ExecutionPolicy Bypass -File tools\check_platform.ps1     문제가 있으면 종료 코드 1
# OS API(헤더 include, 대표 Win32·POSIX 함수)는 engine/src/mealkit/platform/<os>/ 안에서만 쓴다.
# 그래픽 API(d3d12·dxgi·vulkan 헤더)는 RHI 백엔드(engine/src/mealkit/rhi/<api>/) 안에서만 쓴다. 백엔드는 창 핸들·이벤트를
# 다루므로 OS 헤더도 include할 수 있지만, 아래 OS 함수(문자열 변환·파일·DLL 등)는 플랫폼 계층 것을 쓴다.
# 게임 코드(samples/*)는 OS API를 쓰지 않는다 (GameContext로 받는다).
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$engineSrc = Join-Path $root "engine\src\mealkit"

$osHeaders = 'windows\.h|winsock2?\.h|ws2tcpip\.h|mstcpip\.h|shobjidl\.h|shlobj\.h|shellapi\.h|commdlg\.h|wincodec\.h|dwmapi\.h|' +
    'unistd\.h|pthread\.h|dlfcn\.h|fcntl\.h|netdb\.h|sys/\w+\.h|arpa/\w+\.h|' +
    'netinet/\w+\.h|mach-o/\w+\.h|xaudio2\.h|xinput\.h'
$graphicsHeaders = 'd3d12\w*\.h|dxgi\w*\.h|d3dcompiler\.h|wrl/\w+\.h|vulkan/\w+\.h|volk\.h|vk_mem_alloc\.h|imgui_impl_(dx12|vulkan)\.h'
$includePattern = "^\s*#\s*include\s*<($osHeaders)>"
$graphicsPattern = "^\s*#\s*include\s*<($graphicsHeaders)>"
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
    $rhiBackend = $rel -match '^engine\\src\\mealkit\\rhi\\\w+\\'
    $n = 0
    foreach ($line in [IO.File]::ReadAllLines($f.FullName)) {
        $n++
        $code = ($line -replace '//.*$', '')
        if ($code -cmatch $graphicsPattern) {
            if (-not $rhiBackend) { $problems += "${rel}:${n}: graphics API header <$($Matches[1])> outside rhi/<api>/" }
        }
        elseif ($code -cmatch $includePattern) {
            if (-not $rhiBackend) { $problems += "${rel}:${n}: OS header <$($Matches[1])> outside platform/<os>/" }
        }
        elseif ($code -cmatch $callPattern) { $problems += "${rel}:${n}: OS call $($Matches[1])() outside platform/<os>/" }
    }
}

if ($problems.Count -gt 0) {
    $problems | ForEach-Object { Write-Host $_ }
    Write-Host "FAIL: $($problems.Count) OS or graphics API use(s) outside the platform layer and RHI backends" -ForegroundColor Red
    exit 1
}
Write-Host "OK: $($files.Count) files, OS APIs only in platform/<os>/, graphics APIs only in rhi/<api>/" -ForegroundColor Green
exit 0

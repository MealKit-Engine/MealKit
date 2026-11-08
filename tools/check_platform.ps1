# Platform layer check (docs/CODING_MODEL.md "Graphics APIs and OS features"). Called by check.ps1.
#   powershell -ExecutionPolicy Bypass -File tools\check_platform.ps1     exit code 1 on problems
# OS APIs (header includes, representative Win32/POSIX functions) are used only inside engine/core/platform/src/mealkit/platform/<os>/.
# Graphics APIs (d3d12/dxgi/vulkan headers) are used only inside RHI backends (modules/rhi/src/mealkit/rhi/<api>/). Backends handle window handles/events,
# so they may include OS headers too, but for the OS functions below (string conversion, files, DLLs etc.) they use the platform layer's.
# Game code (samples/*) doesn't use OS APIs (it receives them via GameContext).
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
. "$PSScriptRoot\engine_modules_common.ps1"

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
$files += Get-EngineModuleFiles $root |
    Where-Object { $_.FullName -notmatch '\\mealkit\\platform\\(win32|posix)\\' }
$files += Get-ChildItem -Path (Join-Path $root "samples") -Recurse -Include *.cpp, *.h

$problems = @()
foreach ($f in $files) {
    $rel = $f.FullName.Substring($root.Length + 1)
    $rhiBackend = $rel -match '^modules\\rhi\\src\\mealkit\\rhi\\\w+\\'
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

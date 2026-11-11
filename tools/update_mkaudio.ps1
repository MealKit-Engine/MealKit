# Pulls the audio runtime mkaudio (separate repo MkAudio) into the engine's third_party/mkaudio/ (docs/CODING_MODEL.md "Placing external libraries").
#   powershell -ExecutionPolicy Bypass -File tools\update_mkaudio.ps1 -From D:\MkAudio [-Ref v0.1.0]
# - Extracts the contents of -Ref (tag/commit, default HEAD) with git archive, so files being edited in the checkout don't come in
# - What's pulled: CMakeLists.txt, include/, src/, LICENSE (tests, CI, and the repo's miniaudio/stb are excluded; the engine's are used)
# - Changes version in third_party/mkaudio/mealkit.txt to "<tag or CMake version> (<commit>)" and runs tools\gen_notices.ps1
# While editing the runtime and engine together, don't pull; point to the checkout with the CMake variable MEALKIT_MKAUDIO_DIR:
#   cmake ... -DMEALKIT_MKAUDIO_DIR=D:/MealKit-Engine/MkAudio   (when done, tag the runtime repo and pull it with this script)
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

# Extract (temp folder → replaces the target folder except mealkit.txt)
$tmp = Join-Path ([IO.Path]::GetTempPath()) ("mkaudio-" + [guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory $tmp | Out-Null
try {
    $tar = Join-Path $tmp "src.tar"
    & git -C $From archive --format=tar -o $tar $commit CMakeLists.txt include src LICENSE
    if ($LASTEXITCODE -ne 0) { throw "git archive failed" }
    & (Join-Path $env:SystemRoot "System32\tar.exe") -xf $tar -C $tmp # Windows tar (Git's tar reads C: paths as hosts)
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

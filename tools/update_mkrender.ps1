# Pulls the renderer libraries (separate repo MkRender: mkrhi, mkrender) into the engine's third_party/mkrender/ (docs/CODING_MODEL.md "Placing external libraries").
#   powershell -ExecutionPolicy Bypass -File tools\update_mkrender.ps1 -From D:\MealKit-Engine\MkRender [-Ref v0.1.0]
# - Extracts the contents of -Ref (tag/commit, default HEAD) with git archive, so files being edited in the checkout don't come in
# - What's pulled: CMakeLists.txt, include/, src/, shaders/ (when present), LICENSE (tests, CI and build files are excluded)
# - Changes version in third_party/mkrender/mealkit.txt to "<tag or CMake version> (<commit>)" and runs tools\gen_notices.ps1
# While editing the libraries and engine together, don't pull; point to the checkout with the CMake variable MEALKIT_MKRENDER_DIR:
#   cmake ... -DMEALKIT_MKRENDER_DIR=D:/MealKit-Engine/MkRender   (when done, tag the MkRender repo and pull it with this script)
param(
    [Parameter(Mandatory = $true)][string]$From,
    [string]$Ref = "HEAD"
)
$ErrorActionPreference = "Stop"
$engine = Split-Path -Parent $PSScriptRoot
$From = (Resolve-Path $From).Path
$target = Join-Path $engine "third_party\mkrender"
$utf8 = New-Object System.Text.UTF8Encoding $false

$commit = (& git -C $From rev-parse --short "$Ref^{commit}").Trim()
if ($LASTEXITCODE -ne 0) { throw "not a git commit in ${From}: $Ref" }
$tag = (& cmd /c "git -C `"$From`" describe --tags --exact-match $commit 2>nul") # cmd: an untagged commit's stderr isn't an error here
if ($LASTEXITCODE -ne 0) { $tag = "" }
$cmake = (& git -C $From show "${commit}:CMakeLists.txt") -join "`n"
if ($cmake -notmatch 'project\(mkrender VERSION ([0-9.]+)') { throw "CMakeLists.txt has no project(mkrender VERSION ...)" }
$version = if ($tag) { $tag.Trim() } else { "v$($Matches[1])+$commit" }

$paths = @("CMakeLists.txt", "include", "src", "LICENSE")
& cmd /c "git -C `"$From`" cat-file -e ${commit}:shaders 2>nul"
if ($LASTEXITCODE -eq 0) { $paths += "shaders" }

# Extract (temp folder -> replaces the target folder except mealkit.txt)
$tmp = Join-Path ([IO.Path]::GetTempPath()) ("mkrender-" + [guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory $tmp | Out-Null
try {
    $tar = Join-Path $tmp "src.tar"
    & git -C $From archive --format=tar -o $tar $commit @paths
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
Write-Host "third_party/mkrender <- $From $version ($commit)"
& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot "gen_notices.ps1")
exit $LASTEXITCODE

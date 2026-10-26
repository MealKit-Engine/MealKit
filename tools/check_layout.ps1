# 프로젝트 폴더 구조 검사 (docs/CODING_MODEL.md "프로젝트 폴더 구조", "외부 라이브러리 두기"). check.ps1·Export·에디터(프로젝트를 열 때)가 같이 쓴다.
#   powershell -ExecutionPolicy Bypass -File tools\check_layout.ps1 [-Project <폴더>]    기본은 엔진 저장소(예제 프로젝트)
# 출력: "error: <경로>: <내용>" / "warning: <경로>: <내용>" 줄, 마지막에 "OK layout ..." 또는 "FAIL layout ...". 오류가 있으면 종료 코드 1.
# - assets/ 바로 밑: 종류 폴더와 정해진 단일 파일만
# - 종류 폴더 안: 그 종류의 확장자만 (하위 폴더는 자유)
# - 외부 도구 원본(.blend, .psd ...)은 assets/에 두지 않는다 → art/
# - third_party/<lib>/: LICENSE와 mealkit.txt(name·version·license·source), 허용 라이선스, 엔진에 이미 있는 라이브러리 금지
# - 정해진 폴더가 없으면 경고 (tools\upgrade_project.ps1이 만든다)
param(
    [string]$Project = ""
)
$ErrorActionPreference = "Stop"
$engine = (Resolve-Path (Split-Path -Parent $PSScriptRoot)).Path
if ($Project -eq "") { $Project = $engine }
$Project = (Resolve-Path $Project).Path.TrimEnd("\")
$isEngine = $Project -ieq $engine
$utf8 = New-Object System.Text.UTF8Encoding $false

# 종류 폴더 → 확장자 (CODING_MODEL "프로젝트 폴더 구조")
$kinds = [ordered]@{
    "scenes"   = @(".scene")
    "models"   = @(".glb", ".gltf", ".bin")
    "textures" = @(".png", ".jpg", ".jpeg", ".hdr", ".dds", ".ktx2")
    "sounds"   = @(".wav", ".ogg")
    "effects"  = @(".efkefc", ".efk", ".fx", ".png")
    "ui"       = @(".ui", ".png")
    "fonts"    = @(".ttf", ".otf", ".txt")
    "tables"   = @(".table")
    "strings"  = @(".txt")
    "lut"      = @(".cube")
}
$singleFiles = @("game.tuning", "view.tuning", "input.bindings", "feedback.table", "sounds.events")
$sourceFormats = @(".blend", ".blend1", ".psd", ".psb", ".kra", ".xcf", ".penpot", ".fig", ".spp", ".sbs", ".sbsar",
    ".max", ".ma", ".mb", ".fbx", ".obj", ".aup3", ".rpp", ".flp", ".als", ".efkproj", ".drp", ".zip", ".7z")
$alwaysOk = @(".gitkeep", ".autosave")
$licenses = @("MIT", "MIT-0", "BSD-2-Clause", "BSD-3-Clause", "Zlib", "Unlicense", "PublicDomain", "CC0-1.0", "OFL-1.1", "BSL-1.0", "ISC")
$sdkFolders = @("steamworks")
$topFolders = @("source", "assets", "art", "modules", "third_party")

$errors = @()
$warnings = @()
function Rel([string]$path) { return $path.Substring($Project.Length + 1).Replace("\", "/") }
function Err([string]$path, [string]$msg) { $script:errors += "error: $(Rel $path): $msg" }
function Warn([string]$path, [string]$msg) { $script:warnings += "warning: $(Rel $path): $msg" }

# ---------------------------------------------------------------- 정해진 폴더
$wanted = if ($isEngine) { @("assets") } else { $topFolders }
foreach ($f in $wanted) {
    if (-not (Test-Path (Join-Path $Project $f))) { $warnings += "warning: $($f)/: missing folder (tools\upgrade_project.ps1 creates it)" }
}

# ---------------------------------------------------------------- assets/
$assets = Join-Path $Project "assets"
if (Test-Path $assets) {
    foreach ($k in $kinds.Keys) {
        if (-not (Test-Path (Join-Path $assets $k))) { $warnings += "warning: assets/$($k)/: missing kind folder (tools\upgrade_project.ps1 creates it)" }
    }
    foreach ($item in Get-ChildItem $assets -Force) {
        if ($item.PSIsContainer) {
            if (-not $kinds.Contains($item.Name.ToLowerInvariant())) {
                Err $item.FullName "unknown folder in assets/ (kind folders: $($kinds.Keys -join ', '))"
            }
        } elseif ($alwaysOk -notcontains $item.Extension.ToLowerInvariant() -and $singleFiles -notcontains $item.Name.ToLowerInvariant()) {
            Err $item.FullName "files directly in assets/ are only $($singleFiles -join ', '). Move it into a kind folder"
        }
    }
    foreach ($file in Get-ChildItem $assets -Recurse -File -Force) {
        $ext = $file.Extension.ToLowerInvariant()
        if ($alwaysOk -contains $ext -or $file.Name -ieq ".gitkeep") { continue }
        if ($sourceFormats -contains $ext) {
            Err $file.FullName "source files of external tools go in art/ (assets/ ships with the game)"
            continue
        }
        $relParts = (Rel $file.FullName).Split("/")
        if ($relParts.Count -lt 3) { continue } # assets 바로 밑 파일은 위에서 봤다
        $kind = $relParts[1].ToLowerInvariant()
        if ($kinds.Contains($kind) -and $kinds[$kind] -notcontains $ext) {
            Err $file.FullName "'$ext' does not belong in assets/$kind/ (expected $($kinds[$kind] -join ' '))"
        }
    }
}

# ---------------------------------------------------------------- third_party/
function Read-Meta([string]$dir) {
    $meta = @{}
    foreach ($line in [IO.File]::ReadAllLines((Join-Path $dir "mealkit.txt"), $utf8)) {
        if ($line -match '^\s*#' -or $line.Trim() -eq "") { continue }
        if ($line -match '^\s*([A-Za-z][A-Za-z0-9]*)\s+(.*?)\s*$') { $meta[$Matches[1]] = $Matches[2] }
    }
    return $meta
}

function Check-ThirdParty([string]$root, [bool]$game) {
    if (-not (Test-Path $root)) { return }
    $engineLibs = @(Get-ChildItem (Join-Path $engine "third_party") -Directory | ForEach-Object { $_.Name.ToLowerInvariant() })
    foreach ($d in Get-ChildItem $root -Directory) {
        if (-not $game -and $sdkFolders -contains $d.Name) { continue }
        if ($game -and $engineLibs -contains $d.Name.ToLowerInvariant()) {
            Err $d.FullName "the engine already has this library (third_party/$($d.Name) in the engine) - do not add it again"
            continue
        }
        if (-not (Test-Path (Join-Path $d.FullName "mealkit.txt"))) {
            Err $d.FullName "mealkit.txt is missing (name, version, license, licenseFile, source, usedIn, kept)"
            continue
        }
        $meta = Read-Meta $d.FullName
        foreach ($key in "version", "license", "source") {
            if (-not $meta.ContainsKey($key) -or $meta[$key] -eq "") { Err (Join-Path $d.FullName "mealkit.txt") "missing '$key'" }
        }
        $licenseFile = if ($meta.ContainsKey("licenseFile")) { $meta["licenseFile"] } else { "LICENSE" }
        if (-not (Test-Path (Join-Path $d.FullName $licenseFile))) { Err $d.FullName "license file '$licenseFile' is missing" }
        if ($meta.ContainsKey("license") -and $licenses -notcontains $meta["license"]) {
            Err (Join-Path $d.FullName "mealkit.txt") "license '$($meta['license'])' is not on the allowed list ($($licenses -join ', '))"
        }
    }
}
Check-ThirdParty (Join-Path $Project "third_party") (-not $isEngine)

foreach ($w in $warnings) { Write-Host $w -ForegroundColor Yellow }
foreach ($e in $errors) { Write-Host $e -ForegroundColor Red }
if ($errors.Count) {
    Write-Host "FAIL layout: $($errors.Count) error(s), $($warnings.Count) warning(s) in $Project (rules: docs/CODING_MODEL.md, project folder layout)" -ForegroundColor Red
    exit 1
}
Write-Host "OK layout: $Project ($($warnings.Count) warning(s))"
exit 0

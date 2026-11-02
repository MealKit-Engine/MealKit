# Project folder layout check (docs/CODING_MODEL.md "Project folder layout", "Placing external libraries"). Shared by check.ps1, Export, and the editor (when opening a project).
#   powershell -ExecutionPolicy Bypass -File tools\check_layout.ps1 [-Project <folder>]    default is the engine repo (sample project)
# Output: "error: <path>: <message>" / "warning: <path>: <message>" lines, then "OK layout ..." or "FAIL layout ...". Exit code 1 on errors.
# - Directly under assets/: only kind folders and designated single files
# - Inside kind folders: only that kind's extensions (subfolders are free)
# - External tool sources (.blend, .psd ...) don't go in assets/ → art/
# - modules/<name>/: module.txt (name = folder name), module assets in modules/<name>/assets/<kind>/ (same rules as above, docs/MODULES.md)
# - third_party/<lib>/: LICENSE and mealkit.txt (name/version/license/source), permissive licenses, no libraries the engine already has
# - Warns if designated folders are missing (tools\upgrade_project.ps1 creates them)
param(
    [string]$Project = ""
)
$ErrorActionPreference = "Stop"
$engine = (Resolve-Path (Split-Path -Parent $PSScriptRoot)).Path
if ($Project -eq "") { $Project = Join-Path $engine "samples\sandbox" }
$Project = (Resolve-Path $Project).Path.TrimEnd("\")
# The engine repo's reference samples (samples\<name>): code is in sim/ view/ app/ next to assets/, libraries are the engine's
$isEngine = (Split-Path -Parent $Project) -ieq (Join-Path $engine "samples")
$utf8 = New-Object System.Text.UTF8Encoding $false

# Kind folder → extensions (CODING_MODEL "Project folder layout")
$kinds = [ordered]@{
    "scenes"   = @(".scene")
    "prefabs"  = @(".prefab")
    "models"   = @(".glb", ".gltf", ".bin")
    "textures" = @(".png", ".jpg", ".jpeg", ".hdr", ".dds", ".ktx2")
    "sounds"   = @(".wav", ".ogg")
    "effects"  = @(".efkefc", ".efk", ".fx", ".png")
    "ui"       = @(".ui", ".png")
    "fonts"    = @(".ttf", ".otf", ".txt")
    "tables"   = @(".table")
    "strings"  = @(".txt")
    "lut"      = @(".cube")
    "probes"   = @(".mkprobe")
    "terrain"  = @(".png", ".scatter")
}
$singleFiles = @("game.tuning", "view.tuning", "input.bindings", "feedback.table", "sounds.events")
$sourceFormats = @(".blend", ".blend1", ".psd", ".psb", ".kra", ".xcf", ".penpot", ".fig", ".spp", ".sbs", ".sbsar",
    ".max", ".ma", ".mb", ".fbx", ".obj", ".aup3", ".rpp", ".flp", ".als", ".efkproj", ".drp", ".zip", ".7z")
$alwaysOk = @(".gitkeep", ".autosave")
$licenses = @("MIT", "MIT-0", "BSD-2-Clause", "BSD-3-Clause", "Zlib", "Unlicense", "PublicDomain", "CC0-1.0", "OFL-1.1", "BSL-1.0", "ISC")
$sdkFolders = @("steamworks", "eos")
$topFolders = @("source", "assets", "art", "modules", "third_party")

$errors = @()
$warnings = @()
function Rel([string]$path) { return $path.Substring($Project.Length + 1).Replace("\", "/") }
function Err([string]$path, [string]$msg) { $script:errors += "error: $(Rel $path): $msg" }
function Warn([string]$path, [string]$msg) { $script:warnings += "warning: $(Rel $path): $msg" }

# ---------------------------------------------------------------- Designated folders
$wanted = if ($isEngine) { @("assets") } else { $topFolders }
foreach ($f in $wanted) {
    if (-not (Test-Path (Join-Path $Project $f))) { $warnings += "warning: $($f)/: missing folder (tools\upgrade_project.ps1 creates it)" }
}

# ---------------------------------------------------------------- assets/ (project, and modules/<name>/assets/ per module)
function Check-Assets([string]$assets, [bool]$wantKinds) {
    if (-not (Test-Path $assets)) { return }
    $label = (Rel $assets) + "/"
    $depth = $label.Split("/").Count - 1 # which path segment the kind folder is
    if ($wantKinds) {
        foreach ($k in $kinds.Keys) {
            if (-not (Test-Path (Join-Path $assets $k))) { $script:warnings += "warning: $label$($k)/: missing kind folder (tools\upgrade_project.ps1 creates it)" }
        }
    }
    foreach ($item in Get-ChildItem $assets -Force) {
        if ($item.PSIsContainer) {
            if (-not $kinds.Contains($item.Name.ToLowerInvariant())) {
                Err $item.FullName "unknown folder in $label (kind folders: $($kinds.Keys -join ', '))"
            }
        } elseif ($alwaysOk -notcontains $item.Extension.ToLowerInvariant() -and $singleFiles -notcontains $item.Name.ToLowerInvariant()) {
            Err $item.FullName "files directly in $label are only $($singleFiles -join ', '). Move it into a kind folder"
        }
    }
    foreach ($file in Get-ChildItem $assets -Recurse -File -Force) {
        $ext = $file.Extension.ToLowerInvariant()
        if ($alwaysOk -contains $ext -or $file.Name -ieq ".gitkeep") { continue }
        if ($sourceFormats -contains $ext) {
            Err $file.FullName "source files of external tools go in art/ ($label ships with the game)"
            continue
        }
        $relParts = (Rel $file.FullName).Split("/")
        if ($relParts.Count -lt $depth + 2) { continue } # files directly under assets were checked above
        $kind = $relParts[$depth].ToLowerInvariant()
        if ($kinds.Contains($kind) -and $kinds[$kind] -notcontains $ext) {
            Err $file.FullName "'$ext' does not belong in $label$kind/ (expected $($kinds[$kind] -join ' '))"
        }
    }
}
Check-Assets (Join-Path $Project "assets") $true

# ---------------------------------------------------------------- modules/ (docs/MODULES.md)
# Per folder: module.txt (name = folder name), only designated items at the module top level, assets in the module's assets/<kind>/ (same rules as above)
$moduleTop = @("module.txt", "module.files", "sim", "view", "assets", "art", "docs", "tests", ".github", ".gitignore", ".gitattributes", "license", "license.md", "license.txt", "readme.md")
$modulesDir = Join-Path $Project "modules"
if (Test-Path $modulesDir) {
    foreach ($m in Get-ChildItem $modulesDir -Directory -Force) {
        $metaFile = Join-Path $m.FullName "module.txt"
        if (-not (Test-Path $metaFile)) { Err $m.FullName "module.txt is missing (name, version, engine, requires)"; continue }
        $name = ""
        foreach ($line in [IO.File]::ReadAllLines($metaFile, $utf8)) {
            if ($line -match '^\s*name\s+(\S+)') { $name = $Matches[1] }
        }
        if ($name -cne $m.Name) { Err $metaFile "'name $name' must match the folder name '$($m.Name)'" }
        foreach ($item in Get-ChildItem $m.FullName -Force) {
            if ($moduleTop -notcontains $item.Name.ToLowerInvariant() -and $item.Extension -ne ".new") {
                Warn $item.FullName "unknown item in a module folder (expected $($moduleTop[0..7] -join ', '))"
            }
        }
        Check-Assets (Join-Path $m.FullName "assets") $false
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

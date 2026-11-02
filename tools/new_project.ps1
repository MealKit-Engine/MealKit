# Create a new game project: makes game code + assets + build files in a folder outside the engine.
# Build files are made by filling in templates/project. Game code and assets come from the chosen template:
#   arena (default): a copy of the sample game (samples/sandbox, the engine's assets/, modules/)
#   empty: templates/empty (one character per player on a floor) + the screen flow shared with the sample
#   powershell -ExecutionPolicy Bypass -File tools\new_project.ps1 -Name MyGame [-Path D:\GameEngine\MealKit] [-Template arena|empty]
# Result: <Path>\<Name>\ (build.bat, editor.bat, source\sim|view|app\, assets\). The engine points to this folder (the script's parent folder).
# Hashes of template files are kept in .mealkit-template (later tools\upgrade_project.ps1 replaces only unmodified files with the new template).
param(
    [Parameter(Mandatory = $true)][string]$Name,
    [string]$Path = "D:\GameEngine\MealKit",
    [ValidateSet("arena", "empty")][string]$Template = "arena"
)
$ErrorActionPreference = "Stop"
. "$PSScriptRoot\project_common.ps1"

# The name becomes the exe name and the CMake target name
if ($Name -notmatch '^[A-Za-z][A-Za-z0-9_]*$') {
    Write-Error "Name must start with a letter and use only letters, digits and '_' (it becomes the exe name)"
    exit 1
}
$engine = $MealKitEngine
$dest = Join-Path ([IO.Path]::GetFullPath($Path)) $Name
if ($dest.StartsWith($engine + "\", [StringComparison]::OrdinalIgnoreCase)) {
    Write-Error "Create projects outside the engine folder ($engine)"
    exit 1
}
if ((Test-Path $dest) -and (Get-ChildItem $dest -Force | Select-Object -First 1)) {
    Write-Error "$dest already exists and is not empty"
    exit 1
}

# 1) Build files: copy templates/project with tokens replaced and record hashes
$manifest = @{}
foreach ($entry in (Get-RenderedTemplate $Name).GetEnumerator()) {
    $text = $entry.Value
    if ($entry.Key -eq "mealkit.project") { # the start scene and the template (tools\upgrade_project.ps1 -Source reads it)
        if ($Template -eq "empty") { $text = $text.Replace("scene scenes/arena.scene", "scene scenes/main.scene") }
        $text = $text -replace '(\r?\n)created ', "`$1templateName $Template`$1created "
    }
    Write-TextFile (Join-Path $dest $entry.Key) $text
    if ($entry.Key -ne "mealkit.project") { $manifest[$entry.Key] = Get-TextHash $text }
}
Write-TemplateManifest $dest $manifest

# 2) Game code: the template's sim/ view/ app/ into source/ (namespace sandbox becomes the project name in lower case)
$source = Get-SampleSource $Name $Template
foreach ($entry in $source.GetEnumerator()) { Write-TextFile (Join-Path $dest $entry.Key) $entry.Value }

# 3) Assets: scenes, models, tuning, screens (excluding autosave files)
$assets = Get-SampleAssets $Template
foreach ($entry in $assets.GetEnumerator()) {
    $target = Join-Path $dest $entry.Key
    New-Item -ItemType Directory -Force (Split-Path -Parent $target) | Out-Null
    Copy-Item $entry.Value $target
}
if ($Template -eq "empty") {
    foreach ($entry in (Get-TemplateStrings $Name $source $assets).GetEnumerator()) { Write-TextFile (Join-Path $dest $entry.Key) $entry.Value }
}

# 4) Source modules: modules/<name>/ used by the sample game (including the module.files install record; update later with tools\module.ps1 update)
$modules = Join-Path $engine "samples\sandbox\modules"
if ($Template -eq "arena" -and (Test-Path $modules)) {
    New-Item -ItemType Directory -Force (Join-Path $dest "modules") | Out-Null
    Get-ChildItem $modules -Directory | ForEach-Object { Copy-Item $_.FullName (Join-Path $dest "modules") -Recurse -Force }
}

Write-Host "Created $dest ($Template template, engine $(Get-MealKitVersion))" -ForegroundColor Green
Write-Host "  engine : $engine"
Write-Host "  build  : $dest\build.bat        (first build compiles the engine too, a few minutes)"
Write-Host "  editor : $dest\editor.bat"
Write-Host "  export : $dest\build.bat Export  -> $dest\export\$Name"

# Create a new game project: makes game code + assets + build files in a folder outside the engine.
# Build files are made by filling in templates/project. Game code and assets come from the chosen template:
#   arena (default): a copy of the sample game (samples/sandbox, the engine's assets/, modules/)
#   (the samples are fetched from their repos by tools\sample.ps1 when samples/ lacks them; samples.lock says which version)
#   empty: templates/empty (one character per player on a floor, the scene's cameras, no menus: the match starts at once)
#   empty2d: templates/empty2d (2D side view on the XY plane, fixed orthographic camera, mealkit.project view2d)
#   coop: a copy of the co-op sample (samples/coop)
#   <git address or folder of a sample repo>: a copy of that sample like arena (its sim/ view/ app/ assets/ modules/; its tag and commit go to .mealkit-template)
#   powershell -ExecutionPolicy Bypass -File tools\new_project.ps1 -Name MyGame [-Path <folder, default: games\ beside the engine>] [-Template arena|empty|empty2d|coop|<sample repo address or folder>] [-Modules screens,matchflow,camrig]
#   -Modules: source modules to add (registry names, module folders or git addresses; tools\module.ps1 add). The screen flow (screens),
#   rounds and countdown (matchflow) and camera controls (camrig) are modules since 2026-11-15; each module's docs/index.md "Adding" says how to connect it.
# Result: <Path>\<Name>\ (build.bat, editor.bat, source\sim|view|app\, assets\). The engine points to this folder (the script's parent folder).
# Hashes of template files are kept in .mealkit-template (later tools\upgrade_project.ps1 replaces only unmodified files with the new template).
param(
    [Parameter(Mandatory = $true)][string]$Name,
    [string]$Path = "",
    [string]$Template = "arena",
    [string[]]$Modules = @()
)
$ErrorActionPreference = "Stop"
. "$PSScriptRoot\project_common.ps1"

# The name becomes the exe name and the CMake target name
if ($Name -notmatch '^[A-Za-z][A-Za-z0-9_]*$') {
    Write-Error "Name must start with a letter and use only letters, digits and '_' (it becomes the exe name)"
    exit 1
}
$engine = $MealKitEngine
# Another sample repo (a folder or git address) is the template "sample"; the name is kept for the messages
$templateRef = $Template
$sampleInfo = $null
if ($MealKitTemplates -notcontains $Template) { $Template = "sample" }
if ($Template -ne "empty" -and $Template -ne "empty2d") { $sampleInfo = Resolve-TemplateSample $Template $templateRef }
if (-not $Path) { $Path = Join-Path (Split-Path -Parent $engine) "games" }
$dest = Join-Path ([IO.Path]::GetFullPath($Path)) $Name
if ($dest.StartsWith($engine + $MealKitSep, $MealKitPathCmp)) {
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
        if (Test-FolderTemplate $Template) { $text = $text.Replace("scene scenes/arena.scene", "scene scenes/main.scene") }
        if ($Template -eq "coop" -or $Template -eq "sample") { $text = $text.Replace("scene scenes/arena.scene", "scene " + (Read-ProjectFile (Get-TemplateSampleDir $Template))["scene"]) }
        if ($Template -eq "empty2d") { $text = $text -replace '(\r?\n)created ', "`$1view2d true`$1lightingModel Unlit`$1created " }
        $text = $text -replace '(\r?\n)created ', "`$1templateName $Template`$1created "
    }
    Write-TextFile (Join-Rel $dest $entry.Key) $text
    if ($entry.Key -ne "mealkit.project") { $manifest[$entry.Key] = Get-TextHash $text }
}
Write-TemplateManifest $dest $manifest $sampleInfo

# 2) Game code: the template's sim/ view/ app/ into source/ (namespace sandbox becomes the project name in lower case)
$source = Get-SampleSource $Name $Template
foreach ($entry in $source.GetEnumerator()) { Write-TextFile (Join-Rel $dest $entry.Key) $entry.Value }

# 3) Assets: scenes, models, tuning, screens (excluding autosave files)
$assets = Get-SampleAssets $Template
foreach ($entry in $assets.GetEnumerator()) {
    $target = Join-Rel $dest $entry.Key
    New-Item -ItemType Directory -Force (Split-Path -Parent $target) | Out-Null
    Copy-Item $entry.Value $target
}
if (Test-FolderTemplate $Template) {
    foreach ($entry in (Get-TemplateStrings $Name $source $assets).GetEnumerator()) { Write-TextFile (Join-Rel $dest $entry.Key) $entry.Value }
}

# 4) Source modules: modules/<name>/ used by the sample game (including the module.files install record; update later with tools\module.ps1 update)
$sampleModules = if ($Template -eq "arena" -or $Template -eq "sample") { Join-Path (Get-TemplateSampleDir $Template) "modules" } else { "" }
if ($sampleModules -and (Test-Path $sampleModules)) {
    New-Item -ItemType Directory -Force (Join-Path $dest "modules") | Out-Null
    Get-ChildItem $sampleModules -Directory | ForEach-Object { Copy-Item $_.FullName (Join-Path $dest "modules") -Recurse -Force }
}

# 5) Engine modules: every module of the standard preset into modules/<name>/ like source modules (docs/ENGINE_MODULES.md "Module selection").
#    The game keeps the ones it uses: tools\module.ps1 remove audio, add/update for other versions
& $MealKitPs -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot "module.ps1") add -Preset standard -Project $dest | Out-Null
if ($LASTEXITCODE -ne 0) { Write-Error "could not add the engine modules (tools\module.ps1 add -Preset standard -Project $dest)"; exit 1 }

# 6) Source modules picked with -Modules (docs/MODULES.md). The game code connects them (each module's docs/index.md "Adding")
$addedModules = @()
foreach ($m in ($Modules | ForEach-Object { $_ -split "," } | Where-Object { $_ })) {
    & $MealKitPs -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot "module.ps1") add -Project $dest $m | Out-Null
    if ($LASTEXITCODE -ne 0) { Write-Warning "could not add the module '$m' (tools\module.ps1 add -Project $dest $m)" } else { $addedModules += $m }
}

Write-Host "Created $dest ($Template template, engine $(Get-MealKitVersion))" -ForegroundColor Green
if ($sampleInfo) { Write-Host "  sample : $($sampleInfo.Name) $($sampleInfo.Tag) ($(if ($sampleInfo.Commit.Length -ge 9) { $sampleInfo.Commit.Substring(0, 9) } else { $sampleInfo.Commit }))" }
Write-Host "  engine : $engine"
if ($addedModules) { Write-Host "  modules: $($addedModules -join ', ') (connect them as modules\<name>\docs\index.md says)" }
$launch = if ($MealKitSep -eq "\") { "$dest\build.bat" } else { "pwsh $dest/build.ps1" }
Write-Host "  build  : $launch        (first build compiles the engine too, a few minutes)"
if ($MealKitSep -eq "\") { Write-Host "  editor : $dest\editor.bat" }
Write-Host "  export : $launch Export  -> $(Join-Path (Join-Path $dest "export") $Name)"

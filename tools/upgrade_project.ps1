# Upgrades a game project to the current engine's templates.
#   powershell -ExecutionPolicy Bypass -File tools\upgrade_project.ps1 -Project <project folder> [-DryRun] [-Force] [-Source]
#
# - Handles only files managed by the template (build.bat, editor.bat, CMakeLists.txt, CLAUDE.md, README.md, .vscode, .clang-format, etc.).
#   source/ and assets/ aren't touched (except with -Source).
# - Files the user didn't edit (matching the hash in .mealkit-template) are replaced with the new template. Edited files aren't overwritten;
#   the new content is left as <file>.new to show the difference. -Force replaces edited files too.
# - mealkit.project only gets its engine and engineVersion lines changed (name/start scene/game items unchanged, line format unchanged too).
# - Files from before the change are kept in .upgrade_backup\<time>\.
# - -Source: replaces source/ game code with the code of the template the project was made from (mealkit.project "templateName": arena = samples/sandbox,
#   empty = templates/empty + the shared screen flow). Used to move projects that didn't edit the template code to a new engine structure. source/sim/view/app files not in the sample are deleted (backed up). Only missing files are added to assets/ (no overwriting).
param(
    [Parameter(Mandatory = $true)][string]$Project,
    [switch]$DryRun,
    [switch]$Force,
    [switch]$Source
)
$ErrorActionPreference = "Stop"
. "$PSScriptRoot\project_common.ps1"

$Project = (Resolve-Path $Project).Path.TrimEnd("\")
if (-not (Test-Path (Join-Path $Project "mealkit.project"))) { Write-Error "$Project has no mealkit.project"; exit 1 }
if ($Project -ieq $MealKitEngine) { Write-Error "This is the engine repository itself - nothing to upgrade"; exit 1 }

$info = Read-ProjectFile $Project
$name = $info["name"]
if (-not $name) { Write-Error "mealkit.project has no name"; exit 1 }
$version = Get-MealKitVersion
$fromVersion = if ($info.Contains("engineVersion")) { $info["engineVersion"] } else { "(none)" }
Write-Host "Upgrading '$name' ($Project): engine $fromVersion -> $version$(if ($DryRun) { '  [dry run]' })"

$manifest = Read-TemplateManifest $Project
$newManifest = @{}
$backup = Join-Path $Project (".upgrade_backup\" + (Get-Date -Format "yyyyMMdd-HHmmss"))
$changed = 0
$kept = 0

function Backup([string]$rel) {
    $src = Join-Path $Project $rel
    if (-not (Test-Path $src)) { return }
    $dst = Join-Path $backup $rel
    New-Item -ItemType Directory -Force (Split-Path -Parent $dst) | Out-Null
    Copy-Item $src $dst -Force
}

function Report([string]$status, [string]$rel, [ConsoleColor]$color = "Gray") {
    Write-Host ("  {0,-9} {1}" -f $status, $rel) -ForegroundColor $color
}

# ---------------------------------------------------------------- Template files
foreach ($entry in (Get-RenderedTemplate $name).GetEnumerator()) {
    $rel = $entry.Key
    $new = $entry.Value
    $target = Join-Path $Project $rel

    if ($rel -eq "mealkit.project") {
        # Keeps user values (name, scene, game.* etc.) and changes only the engine path and version.
        # Old "key=value" lines keep their format (an edited build.bat may only read the old format).
        $current = [IO.File]::ReadAllText($target, $MealKitUtf8)
        $lines = [Collections.Generic.List[string]]::new()
        $seenEngine = $false; $seenVersion = $false; $separator = " "
        foreach ($line in ($current -split "`r?`n")) {
            if ($line -match '^\s*(engine|engineVersion)(\s*=\s*|\s+|$)') {
                $sep = if ($Matches[2] -match '=') { "=" } else { " " }
                $separator = $sep
                if ($Matches[1] -eq "engine") { $lines.Add("engine$sep$MealKitEngine"); $seenEngine = $true }
                else { $lines.Add("engineVersion$sep$version"); $seenVersion = $true }
            }
            else { $lines.Add($line) }
        }
        while ($lines.Count -gt 0 -and $lines[$lines.Count - 1] -eq "") { $lines.RemoveAt($lines.Count - 1) }
        if (-not $seenEngine) { $lines.Add("engine$separator$MealKitEngine") }
        if (-not $seenVersion) { $lines.Add("engineVersion$separator$version") }
        $merged = ($lines -join "`r`n") + "`r`n"
        if ($merged.Replace("`r`n", "`n") -ne $current.Replace("`r`n", "`n")) {
            Report "updated" $rel Green
            if (-not $DryRun) { Backup $rel; Write-TextFile $target $merged }
            $changed++
        } else { Report "same" $rel }
        continue
    }

    $newHash = Get-TextHash $new
    if (-not (Test-Path $target)) {
        Report "added" $rel Green
        if (-not $DryRun) { Write-TextFile $target $new }
        $newManifest[$rel] = $newHash
        $changed++
        continue
    }
    $current = [IO.File]::ReadAllText($target, $MealKitUtf8)
    $currentHash = Get-TextHash $current
    if ($currentHash -eq $newHash) {
        Report "same" $rel
        $newManifest[$rel] = $newHash
    } elseif ($Force -or ($manifest.Contains($rel) -and $manifest[$rel] -eq $currentHash)) {
        Report "updated" $rel Green
        if (-not $DryRun) { Backup $rel; Write-TextFile $target $new }
        $newManifest[$rel] = $newHash
        $changed++
    } elseif ($manifest.Contains($rel) -and $manifest[$rel] -eq $newHash) {
        # The template is unchanged and only the user edited it: nothing to do
        Report "custom" $rel
        $newManifest[$rel] = $manifest[$rel]
    } else {
        # Files the user edited (or old projects without a hash record): not overwritten, the new content is placed alongside
        $diff = Compare-Object ($current -split "`r?`n") ($new -split "`r?`n")
        Report "modified" "$rel  (kept; new template in $rel.new, $($diff.Count) line(s) differ. -Force to replace)" Yellow
        if (-not $DryRun) { Write-TextFile "$target.new" $new }
        if ($manifest.Contains($rel)) { $newManifest[$rel] = $manifest[$rel] }
        $kept++
    }
}

# ---------------------------------------------------------------- Format migrations
# 0.5.2: sound events assets/sounds.table (engine table) → assets/sounds.events (audio runtime mkaudio format, edited by MealKit Audio).
# Key names are the same, so "row <name>" becomes "event <name>" and the header/format line change. The table is kept in the backup and deleted
$oldSounds = Join-Path $Project "assets\sounds.table"
$newSounds = Join-Path $Project "assets\sounds.events"
if ((Test-Path $oldSounds) -and -not (Test-Path $newSounds)) {
    $out = [Collections.Generic.List[string]]::new()
    $out.Add('# MealKit audio events. Edit them in MealKit Audio or by hand: "event <name>" or "bus <name>", then "key value" lines.')
    $out.Add("@format audio-events 1")
    foreach ($line in [IO.File]::ReadAllLines($oldSounds, $MealKitUtf8)) {
        if ($line -match '^\s*(#|@format)') { continue }
        if ($line -match '^\s*row\s+(.*)$') { $out.Add("event $($Matches[1].Trim())"); continue }
        $out.Add($line)
    }
    Report "converted" "assets\sounds.table -> assets\sounds.events" Green
    if (-not $DryRun) {
        Write-TextFile $newSounds (($out -join "`n") + "`n")
        Backup "assets\sounds.table"
        Remove-Item $oldSounds -Force
    }
    $changed++
}

# ---------------------------------------------------------------- Game code (-Source)
if ($Source) {
    $template = Get-ProjectTemplate $Project
    $sample = Get-SampleSource $name $template
    foreach ($dir in "sim", "view", "app") {
        $folder = Join-Path $Project "source\$dir"
        if (-not (Test-Path $folder)) { continue }
        $base = (Get-Item $folder).FullName # long form: TEMP paths may be 8.3 short paths, FullName below is always long
        Get-ChildItem $folder -Recurse -File | ForEach-Object {
            $rel = "source\$dir\" + $_.FullName.Substring($base.Length + 1)
            if (-not $sample.Contains($rel)) {
                Report "removed" $rel Yellow
                if (-not $DryRun) { Backup $rel; Remove-Item $_.FullName -Force }
                $changed++
            }
        }
    }
    foreach ($entry in $sample.GetEnumerator()) {
        $target = Join-Path $Project $entry.Key
        if ((Test-Path $target) -and (Get-TextHash ([IO.File]::ReadAllText($target, $MealKitUtf8))) -eq (Get-TextHash $entry.Value)) {
            continue
        }
        Report $(if (Test-Path $target) { "updated" } else { "added" }) $entry.Key Green
        if (-not $DryRun) { Backup $entry.Key; Write-TextFile $target $entry.Value }
        $changed++
    }
    # Adds source modules used by the sample that the project lacks (existing modules are updated with tools\module.ps1 update)
    $engineModules = Join-Path $MealKitEngine "modules"
    if ($template -eq "arena" -and (Test-Path $engineModules)) {
        foreach ($m in Get-ChildItem $engineModules -Directory) {
            $target = Join-Path $Project "modules\$($m.Name)"
            if (Test-Path $target) { continue }
            Report "added" "modules\$($m.Name)" Green
            if (-not $DryRun) { New-Item -ItemType Directory -Force (Split-Path -Parent $target) | Out-Null; Copy-Item $m.FullName $target -Recurse }
            $changed++
        }
    }
    # Adds only template assets that the project lacks (e.g. assets/game.tuning)
    foreach ($entry in (Get-SampleAssets $template).GetEnumerator()) {
        $rel = $entry.Key
        $target = Join-Path $Project $rel
        if (-not (Test-Path $target)) {
            Report "added" $rel Green
            if (-not $DryRun) {
                New-Item -ItemType Directory -Force (Split-Path -Parent $target) | Out-Null
                Copy-Item $entry.Value $target
            }
            $changed++
        }
    }
}

if (-not $DryRun) { Write-TemplateManifest $Project $newManifest }
Write-Host "Done: $changed change(s), $kept locally modified file(s) kept$(if ((-not $DryRun) -and $changed -gt 0) { ". Backup: $backup" })"
if ($changed -gt 0 -and -not $DryRun) { Write-Host "Rebuild the game code: $Project\build.bat Editor" }
exit 0

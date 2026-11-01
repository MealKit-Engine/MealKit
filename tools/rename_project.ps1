# Renames a game project (= exe name, CMake target name). Run when the name field is edited in the editor project manager.
#   powershell -ExecutionPolicy Bypass -File tools\rename_project.ps1 -Project <folder> -Name <new name>
# - the name line in mealkit.project (keeping its original format)
# - template files (CMakeLists.txt, build.bat, README.md, CLAUDE.md, .vscode): if unmodified, refilled with the new name;
#   if modified, only the old name (where not surrounded by letters/digits) is replaced. Unmodified files also get new .mealkit-template hashes.
# - source\ code: only where it was put when the project was created ("old name" strings, oldname.exe)
#   and the game namespace (Get-GameNamespace: old name in lower case -> new name in lower case)
# - deletes build\ (CMake cache and outputs contain the old name). export\<old name> is left as is
param(
    [Parameter(Mandatory = $true)][string]$Project,
    [Parameter(Mandatory = $true)][string]$Name
)
$ErrorActionPreference = "Stop"
. "$PSScriptRoot\project_common.ps1"

if ($Name -notmatch '^[A-Za-z][A-Za-z0-9_]*$') {
    Write-Error "Name must start with a letter and use only letters, digits and '_' (it becomes the exe name)"
    exit 1
}
$Project = (Resolve-Path $Project).Path.TrimEnd("\")
if ($Project -ieq $MealKitEngine) {
    Write-Error "The engine's sample project cannot be renamed (samples/sandbox is the template)"
    exit 1
}
$projectFile = Join-Path $Project "mealkit.project"
if (-not (Test-Path $projectFile)) {
    Write-Error "$projectFile not found"
    exit 1
}
$old = (Read-ProjectFile $Project)["name"]
if (-not $old) {
    Write-Error "mealkit.project has no name"
    exit 1
}
if ($old -ceq $Name) {
    Write-Host "Name is already $Name"
    exit 0
}
$word = '(?<![A-Za-z0-9])' + [regex]::Escape($old) + '(?![A-Za-z0-9])'

# 1) mealkit.project: only the name line (keeping its original format)
$lines = [IO.File]::ReadAllLines($projectFile, $MealKitUtf8)
for ($i = 0; $i -lt $lines.Length; $i++) {
    if ($lines[$i] -match '^(\s*name)(\s*=\s*|\s+)(.*?)\s*$') { $lines[$i] = $Matches[1] + $Matches[2] + $Name }
}
Write-TextFile $projectFile (($lines -join "`r`n") + "`r`n")
Write-Host "mealkit.project: name $old -> $Name"

# 2) Template files
$manifest = Read-TemplateManifest $Project
$oldRender = Get-RenderedTemplate $old
$newRender = Get-RenderedTemplate $Name
foreach ($key in $newRender.Keys) {
    if ($key -eq "mealkit.project") { continue }
    $file = Join-Path $Project $key
    if (-not (Test-Path $file)) { continue }
    $text = [IO.File]::ReadAllText($file, $MealKitUtf8)
    $hash = Get-TextHash $text
    $unmodified = $manifest.ContainsKey($key) -and $manifest[$key] -eq $hash
    if ($hash -eq (Get-TextHash $oldRender[$key])) { $newText = $newRender[$key] }
    else { $newText = [regex]::Replace($text, $word, $Name) }
    if ($newText -ne $text) {
        Write-TextFile $file $newText
        $note = if ($unmodified) { "" } else { " (edited file: check it)" }
        Write-Host "  $key$note"
    }
    if ($unmodified) { $manifest[$key] = Get-TextHash $newText }
}
if ($manifest.Count -gt 0) { Write-TemplateManifest $Project $manifest }

# 3) Game code: names inserted by new_project (window title, help, comments). Only inside strings and comments so code identifiers aren't touched
$source = Join-Path $Project "source"
if (Test-Path $source) {
    $source = (Get-Item $source).FullName
    $wordInCode = '(?<![A-Za-z0-9_])' + [regex]::Escape($old) + '(?![A-Za-z0-9_])'
    $stringOrComment = '"(?:[^"\\\r\n]|\\.)*"|//[^\r\n]*'
    Get-ChildItem $source -Recurse -File | Where-Object { $_.Extension -in ".h", ".cpp" } | ForEach-Object {
        $text = [IO.File]::ReadAllText($_.FullName, $MealKitUtf8)
        $newText = [regex]::Replace($text, $stringOrComment, { param($m) [regex]::Replace($m.Value, $wordInCode, $Name) })
        $newText = Rename-GameNamespace $newText (Get-GameNamespace $old) (Get-GameNamespace $Name)
        if ($newText -ne $text) {
            Write-TextFile $_.FullName $newText
            Write-Host ("  source\" + $_.FullName.Substring($source.Length + 1))
        }
    }
}

# 4) Build folder (recreated with the new name on the next build)
$build = Join-Path $Project "build"
if (Test-Path $build) {
    Remove-Item -Recurse -Force $build
    Write-Host "Removed build\ (rebuilt on next open)"
}
if (Test-Path (Join-Path $Project "export\$old")) { Write-Host "export\$old was kept (old release build)" }
Write-Host "Renamed $old -> $Name" -ForegroundColor Green

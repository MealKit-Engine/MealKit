# Functions shared by new_project.ps1 and upgrade_project.ps1 (dot-source: . "$PSScriptRoot\project_common.ps1")
# Files made by filling in the template (templates/project) record their SHA-256 in .mealkit-template.
# Upgrades replace with the new template if it matches this value (the user didn't edit it), and don't overwrite otherwise.

$MealKitEngine = (Resolve-Path (Split-Path -Parent $PSScriptRoot)).Path
. "$PSScriptRoot\lock_repo.ps1"
$MealKitUtf8 = New-Object System.Text.UTF8Encoding $false
$MealKitManifestName = ".mealkit-template"
# The PowerShell running this script (5.1 `powershell` on Windows, `pwsh` on Linux/macOS), for calling the other tools
$MealKitPs = (Get-Process -Id $PID).Path
$MealKitSep = [IO.Path]::DirectorySeparatorChar
# Names of the files in the template manifest and in the maps below are project-relative with '\' on every OS (the format of
# .mealkit-template). ConvertTo-RelKey turns a native path tail into one, Join-Rel joins one onto a folder with the native separator.
function ConvertTo-RelKey([string]$path) { return $path.Replace("/", "\") }
function Join-Rel([string]$base, [string]$rel) { return Join-Path $base $rel.Replace("\", [string]$MealKitSep) }
# Windows paths ignore case, Linux paths don't
$MealKitPathCmp = if ($MealKitSep -eq "\") { [StringComparison]::OrdinalIgnoreCase } else { [StringComparison]::Ordinal }

function Get-MealKitVersion {
    return ([IO.File]::ReadAllText((Join-Path $MealKitEngine "VERSION"))).Trim()
}

function Get-TemplateTokens([string]$name) {
    return @{
        "@NAME@"             = $name
        "@MEALKIT_ROOT@"     = $MealKitEngine
        "@MEALKIT_ROOT_FWD@" = $MealKitEngine.Replace("\", "/")
        "@ENGINE_VERSION@"   = (Get-MealKitVersion)
        "@CREATED@"          = (Get-Date -Format "yyyy-MM-dd") # written only to mealkit.project (a file that isn't hashed)
    }
}

# Filled-in template file contents: project-relative path → text
function Get-RenderedTemplate([string]$name) {
    $tokens = Get-TemplateTokens $name
    $template = Join-Path $MealKitEngine "templates/project"
    $files = [ordered]@{}
    Get-ChildItem $template -Recurse -File -Force | Sort-Object FullName | ForEach-Object {
        $text = [IO.File]::ReadAllText($_.FullName, $MealKitUtf8)
        foreach ($k in $tokens.Keys) { $text = $text.Replace($k, $tokens[$k]) }
        $files[(ConvertTo-RelKey $_.FullName.Substring($template.Length + 1))] = $text
    }
    return $files
}

# Content hash independent of line endings (CRLF/LF)
function Get-TextHash([string]$text) {
    $bytes = $MealKitUtf8.GetBytes($text.Replace("`r`n", "`n"))
    $sha = [Security.Cryptography.SHA256]::Create()
    return ([BitConverter]::ToString($sha.ComputeHash($bytes))).Replace("-", "").ToLowerInvariant()
}

function Write-TextFile([string]$file, [string]$text) {
    New-Item -ItemType Directory -Force (Split-Path -Parent $file) | Out-Null
    [IO.File]::WriteAllText($file, $text, $MealKitUtf8)
}

function Read-TemplateManifest([string]$project) {
    $map = @{}
    $file = Join-Path $project $MealKitManifestName
    if (Test-Path $file) {
        foreach ($line in [IO.File]::ReadAllLines($file, $MealKitUtf8)) {
            if ($line -match '^([0-9a-f]{64})\s+(.+)$') { $map[$Matches[2]] = $Matches[1] }
        }
    }
    return $map
}

# The sample a project was made from: "@sample <name> <url> <tag> <commit>" in .mealkit-template (null for the empty templates)
function Read-TemplateSample([string]$project) {
    $file = Join-Path $project $MealKitManifestName
    if (-not (Test-Path $file)) { return $null }
    foreach ($line in [IO.File]::ReadAllLines($file, $MealKitUtf8)) {
        if ($line -match '^@sample\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+)\s*$') { return [pscustomobject]@{ Name = $Matches[1]; Url = $Matches[2]; Tag = $Matches[3]; Commit = $Matches[4] } }
    }
    return $null
}

function Write-TemplateManifest([string]$project, [hashtable]$map, $sample = $null) {
    $lines = @("# MealKit template files (sha256  path). tools\upgrade_project.ps1 replaces a file only while its hash matches.")
    if ($sample) { $lines += "@sample $($sample.Name) $($sample.Url) $($sample.Tag) $($sample.Commit)" }
    foreach ($k in ($map.Keys | Sort-Object)) { $lines += "$($map[$k])  $k" }
    Write-TextFile (Join-Path $project $MealKitManifestName) (($lines -join "`r`n") + "`r`n")
}

# One mealkit.project line: "path value" or the old "key=value" format (same rules as mealkit/game/ProjectSettings.cpp)
$MealKitProjectLine = '^\s*([A-Za-z_][A-Za-z0-9_.\[\]]*)(?:\s*=\s*|\s+|$)(.*?)\s*$'

# Reads mealkit.project (path → value)
function Read-ProjectFile([string]$project) {
    $values = [ordered]@{}
    foreach ($line in [IO.File]::ReadAllLines((Join-Path $project "mealkit.project"), $MealKitUtf8)) {
        if ($line -match '^\s*#') { continue }
        if ($line -match $MealKitProjectLine) { $values[$Matches[1]] = $Matches[2] }
    }
    return $values
}

# Contents when moving the sample game code (sim/ view/ app/ of samples/sandbox) into the project's source/. Only names in the window title and help are changed.
# C++ namespace for a project's game code: the project name in lower case ("MyGame" -> mygame), "_game" added if that is a keyword
function Get-GameNamespace([string]$name) {
    $ns = $name.ToLowerInvariant()
    $keywords = "alignas alignof and asm auto bitand bitor bool break case catch char class compl concept const consteval constexpr constinit " +
        "continue co_await co_return co_yield decltype default delete do double else enum explicit export extern false float for friend goto " +
        "if inline int long mutable namespace new noexcept not nullptr operator or private protected public register requires return short " +
        "signed sizeof static struct switch template this throw true try typedef typeid typename union unsigned using virtual void volatile " +
        "while xor std mealkit"
    if (($keywords -split " ") -contains $ns) { $ns += "_game" }
    return $ns
}

# Renames namespace $from to $to in C++ code: "namespace x", "// namespace x" and "x::" (other uses of the word are left alone)
function Rename-GameNamespace([string]$text, [string]$from, [string]$to) {
    $f = [regex]::Escape($from)
    $text = [regex]::Replace($text, '(?<=\bnamespace\s+)' + $f + '(?![A-Za-z0-9_])', $to)
    return [regex]::Replace($text, '(?<![A-Za-z0-9_:])' + $f + '(?=::)', $to)
}

# ---- Samples (docs/DECISIONS.md 2026-11-24 "The reference samples leave the engine repo"): arena = samples/sandbox and coop = samples/coop come from
# samples.lock through tools\sample.ps1 (synced when the folder is missing). -Template can also name another sample repo (a git address or a folder):
# template "sample", described by $MealKitSample (Name = its project name, Dir, Url, Tag, Commit). The tag and commit go to .mealkit-template.
$MealKitSample = $null
$MealKitLockPath = Join-Path $MealKitEngine "samples.lock"

# samples/<name> of the engine, fetched first when missing; returns the folder
function Get-EngineSampleDir([string]$name) {
    $dir = Join-Path $MealKitEngine "samples/$name"
    if (-not (Test-Path (Join-Path $dir "mealkit.project"))) {
        & $MealKitPs -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot "sample.ps1") sync $name | Out-Host
        if ($LASTEXITCODE -ne 0 -or -not (Test-Path (Join-Path $dir "mealkit.project"))) { throw "sample '$name' is not available (tools\sample.ps1 sync $name)" }
    }
    return $dir
}

function Get-LockedSample([string]$sampleName) {
    $locked = @(Read-LockFile $MealKitLockPath "sample" 5 | Where-Object { $_.Name -eq $sampleName })
    if ($locked.Count -eq 0) { throw "no sample '$sampleName' in samples.lock" }
    return $locked[0]
}

# Describes the sample behind a template and keeps it in $MealKitSample. $reference: a folder or git address of a sample repo (template "sample")
function Resolve-TemplateSample([string]$template, [string]$reference = "") {
    if ($template -eq "sample") {
        $url = $reference
        if (Test-Path $reference) {
            $dir = (Resolve-Path $reference).Path
            $url = $dir # a folder is recorded by its full path
            $tag = "-"
            if (Test-Path (Join-Path $dir ".git")) {
                $t = (& git -C $dir describe --tags --exact-match 2>$null)
                if ($t) { $tag = @($t)[0] }
                $commit = (& git -C $dir rev-parse HEAD).Trim()
            } else { $commit = "-" }
        } elseif ($reference -match '^(https?|ssh|git)://|^git@') {
            $dir = Get-RepoDir ([pscustomobject]@{ Url = $reference }) (Split-Path -Parent $MealKitEngine) "samples"
            $tag = Newest-Tag $dir
            $target = if ($tag) { $tag } else { "origin/HEAD" }
            & git -C $dir checkout --quiet --detach $target
            if ($LASTEXITCODE -ne 0) { throw "could not check out $target in $dir" }
            if (-not $tag) { $tag = "-" }
            $commit = (& git -C $dir rev-parse HEAD).Trim()
        } else { throw "-Template '$reference' is neither a template name nor a sample folder or git address" }
    } else {
        $sampleName = if ($template -eq "coop") { "coop" } else { "sandbox" }
        $dir = Get-EngineSampleDir $sampleName
        $lock = Get-LockedSample $sampleName
        $url = $lock.Url; $tag = $lock.Tag; $commit = $lock.Commit
    }
    if (-not (Test-Path (Join-Path $dir "mealkit.project"))) { throw "$dir has no mealkit.project: not a sample" }
    $script:MealKitSample = [pscustomobject]@{ Name = (Read-ProjectFile $dir)["name"]; Dir = $dir; Url = $url; Tag = $tag; Commit = $commit }
    return $script:MealKitSample
}

# The sample folder a template copies from (the sample of "sample" must have been resolved)
function Get-TemplateSampleDir([string]$template) {
    if ($template -eq "sample") {
        if (-not $script:MealKitSample) { throw "no sample resolved for template 'sample'" }
        return $script:MealKitSample.Dir
    }
    return Get-EngineSampleDir $(if ($template -eq "coop") { "coop" } else { "sandbox" })
}

# Project templates (tools\new_project.ps1 -Template): arena = the sample game (samples/sandbox + the engine's assets/),
# empty = templates/empty (source/ + assets/) plus the screens engine modules open ($MealKitSharedFlowAssets: settings, subtitles, theme, fonts),
# empty2d = templates/empty2d the same way (side view on the XY plane, mealkit.project view2d), coop = a copy of samples/coop.
$MealKitTemplates = @("arena", "empty", "empty2d", "coop")
# Templates made from a folder under templates\ (own source/ + assets/, the shared engine screens, generated strings)
function Test-FolderTemplate([string]$template) { return $template -eq "empty" -or $template -eq "empty2d" }
# The sample a copied template comes from, and its C++ namespace
function Get-TemplateSample([string]$template) {
    if ($template -eq "sample") { return $script:MealKitSample.Name }
    if ($template -eq "coop") { return "coop" }
    return "sandbox"
}
# The screen flow (title, menu, lobby, results) is the screens source module since 2026-11-15 (docs/DECISIONS.md
# "Templates carry no game shape"), so no game code is shared: new_project.ps1 -Modules screens adds it
$MealKitSharedFlowSource = @()
$MealKitSharedFlowAssets = @("ui/settings.ui", "ui/subtitles.ui", "ui/theme.ui", "fonts")

# The template a project was made from (mealkit.project "templateName", arena for projects made before it was written)
function Get-ProjectTemplate([string]$project) {
    $info = Read-ProjectFile $project
    if ($info.Contains("templateName") -and ($MealKitTemplates -contains $info["templateName"] -or $info["templateName"] -eq "sample")) { return $info["templateName"] }
    return "arena"
}

function Convert-GameSource([string]$text, [string]$name, [string]$from = "sandbox") {
    $text = $text.Replace("MealKit Sandbox", $name).Replace("MealKit Coop", $name).Replace("$from.exe", "$name.exe")
    return Rename-GameNamespace $text $from (Get-GameNamespace $name)
}

# Game code of a template: project-relative path ("source\sim\World.h") → text. Template code is written in namespace sandbox like the sample
function Get-SampleSource([string]$name, [string]$template = "arena") {
    $files = [ordered]@{}
    if (Test-FolderTemplate $template) {
        $sample = Get-EngineSampleDir "sandbox"
        $root = Join-Path $MealKitEngine "templates/$template/source"
        Get-ChildItem $root -Recurse -File | Where-Object { $_.Extension -in ".h", ".cpp" } | Sort-Object FullName | ForEach-Object {
            $files["source\" + (ConvertTo-RelKey $_.FullName.Substring($root.Length + 1))] = Convert-GameSource ([IO.File]::ReadAllText($_.FullName, $MealKitUtf8)) $name
        }
        foreach ($rel in $MealKitSharedFlowSource) {
            $files["source\$rel"] = Convert-GameSource ([IO.File]::ReadAllText((Join-Path $sample $rel), $MealKitUtf8)) $name
        }
        return $files
    }
    $sampleName = Get-TemplateSample $template
    $sample = Get-TemplateSampleDir $template
    foreach ($dir in "sim", "view", "app") {
        $from = Join-Path $sample $dir
        Get-ChildItem $from -Recurse -File | Where-Object { $_.Extension -in ".h", ".cpp" } | Sort-Object FullName | ForEach-Object {
            $files["source\$dir\" + (ConvertTo-RelKey $_.FullName.Substring($from.Length + 1))] = Convert-GameSource ([IO.File]::ReadAllText($_.FullName, $MealKitUtf8)) $name $sampleName
        }
    }
    return $files
}

# Assets of a template: project-relative path ("assets\scenes\main.scene") → source file. Text files that are generated (strings) aren't here
function Get-SampleAssets([string]$template = "arena") {
    $files = [ordered]@{}
    $add = {
        param($root, $item)
        Get-ChildItem $item -Recurse -File | Where-Object { $_.Extension -ne ".autosave" } | Sort-Object FullName | ForEach-Object {
            $files["assets\" + (ConvertTo-RelKey $_.FullName.Substring($root.Length + 1))] = $_.FullName
        }
    }
    if (Test-FolderTemplate $template) {
        $engineAssets = Join-Path (Get-EngineSampleDir "sandbox") "assets" # the shared engine screens, fonts and theme
        $own = Join-Path $MealKitEngine "templates/$template/assets"
        & $add $own $own
        foreach ($rel in $MealKitSharedFlowAssets) { & $add $engineAssets (Join-Path $engineAssets $rel) }
    } else {
        $sampleAssets = Join-Path (Get-TemplateSampleDir $template) "assets"
        & $add $sampleAssets $sampleAssets
    }
    return $files
}

# The empty template's strings/<lang>.txt: the engine's lines whose keys the project uses (its .ui files, string literals in its code,
# its input actions) or the engine itself reads (settings., language., title., sub., game.), with game.title set to the project name
function Get-TemplateStrings([string]$name, [Collections.IDictionary]$source, [Collections.IDictionary]$assets) {
    $used = @{}
    foreach ($path in $assets.Values) {
        if ([IO.Path]::GetExtension($path) -ne ".ui") { continue }
        foreach ($line in [IO.File]::ReadAllLines($path, $MealKitUtf8)) {
            if ($line -match '^\s*(key|placeholder)\s+(\S+)') { $used[$Matches[2]] = $true }
            if ($line -match '^\s*select\s+\S+\s+(\S+)') { foreach ($k in $Matches[1] -split ",") { $used[$k] = $true } }
        }
    }
    foreach ($text in $source.Values) { foreach ($m in [regex]::Matches($text, '"([a-z]+(?:\.[A-Za-z]+)+)"')) { $used[$m.Groups[1].Value] = $true } }
    $bindings = $assets["assets\input.bindings"]
    if ($bindings) { foreach ($line in [IO.File]::ReadAllLines($bindings, $MealKitUtf8)) { if ($line -match '^row\s+(\S+)') { $used["action." + $Matches[1]] = $true } } }
    $files = [ordered]@{}
    foreach ($lang in "en", "ko") {
        $out = [Collections.Generic.List[string]]::new()
        foreach ($line in [IO.File]::ReadAllLines((Join-Path (Get-EngineSampleDir "sandbox") "assets/strings/$lang.txt"), $MealKitUtf8)) {
            if ($line -match '^\s*([A-Za-z0-9_.]+)\s*=') {
                $key = $Matches[1]
                if ($key -eq "game.title") { $out.Add("game.title = $name"); continue }
                if (-not ($used.Contains($key) -or $key -match '^(settings|language|title|sub|reject|content)\.')) { continue }
            }
            $out.Add($line)
        }
        $text = ($out -join "`r`n") -replace "(`r`n){3,}", "`r`n`r`n"
        $files["assets\strings\$lang.txt"] = $text.TrimEnd() + "`r`n"
    }
    return $files
}

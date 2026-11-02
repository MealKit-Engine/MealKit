# Functions shared by new_project.ps1 and upgrade_project.ps1 (dot-source: . "$PSScriptRoot\project_common.ps1")
# Files made by filling in the template (templates/project) record their SHA-256 in .mealkit-template.
# Upgrades replace with the new template if it matches this value (the user didn't edit it), and don't overwrite otherwise.

$MealKitEngine = (Resolve-Path (Split-Path -Parent $PSScriptRoot)).Path
$MealKitUtf8 = New-Object System.Text.UTF8Encoding $false
$MealKitManifestName = ".mealkit-template"

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
    $template = Join-Path $MealKitEngine "templates\project"
    $files = [ordered]@{}
    Get-ChildItem $template -Recurse -File -Force | Sort-Object FullName | ForEach-Object {
        $text = [IO.File]::ReadAllText($_.FullName, $MealKitUtf8)
        foreach ($k in $tokens.Keys) { $text = $text.Replace($k, $tokens[$k]) }
        $files[$_.FullName.Substring($template.Length + 1)] = $text
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

function Write-TemplateManifest([string]$project, [hashtable]$map) {
    $lines = @("# MealKit template files (sha256  path). tools\upgrade_project.ps1 replaces a file only while its hash matches.")
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

# Project templates (tools\new_project.ps1 -Template): arena = the sample game (samples/sandbox + the engine's assets/),
# empty = templates/empty (source/ + assets/) plus the screen flow shared with the sample ($MealKitSharedFlow*).
$MealKitTemplates = @("arena", "empty")
$MealKitSharedFlowSource = @("app\Flow.h", "app\Flow.cpp", "app\Screens.h")
$MealKitSharedFlowAssets = @("ui\title.ui", "ui\menu.ui", "ui\join.ui", "ui\lobby.ui", "ui\loading.ui", "ui\pause.ui", "ui\results.ui",
    "ui\message.ui", "ui\settings.ui", "ui\subtitles.ui", "ui\theme.ui", "fonts")

# The template a project was made from (mealkit.project "templateName", arena for projects made before it was written)
function Get-ProjectTemplate([string]$project) {
    $info = Read-ProjectFile $project
    if ($info.Contains("templateName") -and $MealKitTemplates -contains $info["templateName"]) { return $info["templateName"] }
    return "arena"
}

function Convert-GameSource([string]$text, [string]$name) {
    $text = $text.Replace("MealKit Sandbox", $name).Replace("sandbox.exe", "$name.exe")
    return Rename-GameNamespace $text "sandbox" (Get-GameNamespace $name)
}

# Game code of a template: project-relative path ("source\sim\World.h") → text. Template code is written in namespace sandbox like the sample
function Get-SampleSource([string]$name, [string]$template = "arena") {
    $files = [ordered]@{}
    $sample = Join-Path $MealKitEngine "samples\sandbox"
    if ($template -eq "empty") {
        $root = Join-Path $MealKitEngine "templates\empty\source"
        Get-ChildItem $root -Recurse -File | Where-Object { $_.Extension -in ".h", ".cpp" } | Sort-Object FullName | ForEach-Object {
            $files["source\" + $_.FullName.Substring($root.Length + 1)] = Convert-GameSource ([IO.File]::ReadAllText($_.FullName, $MealKitUtf8)) $name
        }
        foreach ($rel in $MealKitSharedFlowSource) {
            $files["source\$rel"] = Convert-GameSource ([IO.File]::ReadAllText((Join-Path $sample $rel), $MealKitUtf8)) $name
        }
        return $files
    }
    foreach ($dir in "sim", "view", "app") {
        $from = Join-Path $sample $dir
        Get-ChildItem $from -Recurse -File | Where-Object { $_.Extension -in ".h", ".cpp" } | Sort-Object FullName | ForEach-Object {
            $files["source\$dir\" + $_.FullName.Substring($from.Length + 1)] = Convert-GameSource ([IO.File]::ReadAllText($_.FullName, $MealKitUtf8)) $name
        }
    }
    return $files
}

# Assets of a template: project-relative path ("assets\scenes\main.scene") → source file. Text files that are generated (strings) aren't here
function Get-SampleAssets([string]$template = "arena") {
    $files = [ordered]@{}
    $engineAssets = Join-Path $MealKitEngine "samples\sandbox\assets"
    $add = {
        param($root, $item)
        Get-ChildItem $item -Recurse -File | Where-Object { $_.Extension -ne ".autosave" } | Sort-Object FullName | ForEach-Object {
            $files["assets\" + $_.FullName.Substring($root.Length + 1)] = $_.FullName
        }
    }
    if ($template -eq "empty") {
        $own = Join-Path $MealKitEngine "templates\empty\assets"
        & $add $own $own
        foreach ($rel in $MealKitSharedFlowAssets) { & $add $engineAssets (Join-Path $engineAssets $rel) }
    } else {
        & $add $engineAssets $engineAssets
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
        foreach ($line in [IO.File]::ReadAllLines((Join-Path $MealKitEngine "samples\sandbox\assets\strings\$lang.txt"), $MealKitUtf8)) {
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

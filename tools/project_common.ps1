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
function Get-SampleSource([string]$name) {
    $sample = Join-Path $MealKitEngine "samples\sandbox"
    $files = [ordered]@{}
    foreach ($dir in "sim", "view", "app") {
        $from = Join-Path $sample $dir
        Get-ChildItem $from -Recurse -File | Where-Object { $_.Extension -in ".h", ".cpp" } | Sort-Object FullName | ForEach-Object {
            $text = [IO.File]::ReadAllText($_.FullName, $MealKitUtf8)
            $text = $text.Replace("MealKit Sandbox", $name).Replace("sandbox.exe", "$name.exe")
            $files["source\$dir\" + $_.FullName.Substring($from.Length + 1)] = $text
        }
    }
    return $files
}

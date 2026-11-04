# Editor UI text check (docs/CODING_MODEL.md "Editor UI strings and languages"). Called by check.ps1.
#   powershell -ExecutionPolicy Bypass -File tools\check_strings.ps1          exit code 1 on problems
#   powershell -ExecutionPolicy Bypass -File tools\check_strings.ps1 -List    prints missing translations as "source = " lines (paste into ko.txt and fill in)
# - Whether all tr()/trl()/trf() sources in editor code, "###" names, and field descriptions/scene kind names declared by the engine (trData) are in engine/res/strings/<language>.txt
# - Lines in the table that aren't used anywhere
# - Whether translations' printf formats (%s, %u ...) match the source
# - Whether Korean strings remain in editor code (writing "i18n-ignore" on that line excludes it)
param(
    [switch]$List
)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$utf8 = New-Object System.Text.UTF8Encoding $false
$editorDir = Join-Path $root "engine\src\mealkit\editor"
$engineDir = Join-Path $root "engine\src\mealkit"
$stringsDir = Join-Path $root "engine\res\strings"

$literal = '"((?:[^"\\]|\\.)*)"'
function Unescape-C([string]$s) {
    return [regex]::Replace($s, '\\(.)', { param($m) switch ($m.Groups[1].Value) { 'n' { "`n" } 't' { "`t" } default { $m.Groups[1].Value } } })
}
# The part before "##" is the translation key (same as translate in Strings.cpp)
function Key-Of([string]$s) {
    $i = $s.IndexOf("##")
    if ($i -ge 0) { return $s.Substring(0, $i) }
    return $s
}

$keys = [ordered]@{}   # key → first place it appeared
$problems = @()
function Add-Key([string]$key, [string]$where) {
    if ($key -ne "" -and -not $keys.Contains($key)) { $keys[$key] = $where }
}

# 1) Editor code
foreach ($file in Get-ChildItem $editorDir -File -Include *.cpp, *.h -Recurse) {
    $raw = [IO.File]::ReadAllText($file.FullName, $utf8)
    $name = $file.Name
    # Comments are ignored (line comments and after " // ")
    $text = [regex]::Replace($raw, '(?m)(^\s*//.*$| // .*$)', '')
    # Concatenated strings like tr("a" "b") count as one
    foreach ($m in [regex]::Matches($text, '\b(?:tr|trl|trf)\(\s*((?:' + $literal + '\s*)+)')) {
        $joined = ([regex]::Matches($m.Groups[1].Value, $literal) | ForEach-Object { $_.Groups[1].Value }) -join ""
        Add-Key (Key-Of (Unescape-C $joined)) $name
    }
    # Window/popup name constants ("Scene Tree###Hierarchy"). If the front part is only a printf format ("%s (%u)###level"), there's nothing to translate
    foreach ($m in [regex]::Matches($text, $literal)) {
        if ($m.Groups[1].Value.Contains("###") -and -not $m.Groups[1].Value.StartsWith("#")) {
            $k = Key-Of (Unescape-C $m.Groups[1].Value)
            if (($k -replace '%[-+ #0]*\d*(?:\.\d+)?[hlzjt]*[diuxXfFeEgGcsp%]', '') -match '[A-Za-z]') { Add-Key $k $name }
        }
    }
    # Command table (EditorCommands.cpp): names in {"name", shortcut, ...} lines and fixed shortcut help lines {"what it does", "key"} are translated with tr(c.name)/tr(row[n])
    if ($name -eq "EditorCommands.cpp") {
        foreach ($m in [regex]::Matches($text, '\{\s*' + $literal + '(?:\s*,\s*' + $literal + '\s*\})?')) {
            Add-Key (Key-Of (Unescape-C $m.Groups[1].Value)) $name
            if ($m.Groups[2].Success) { Add-Key (Key-Of (Unescape-C $m.Groups[2].Value)) $name }
        }
    }
    # Korean strings
    $lineNo = 0
    foreach ($line in $raw -split "`n") {
        $lineNo++
        if ($line.TrimStart().StartsWith("//") -or $line.Contains("i18n-ignore")) { continue }
        $code = $line
        $c = $code.IndexOf(" // ")
        if ($c -ge 0) { $code = $code.Substring(0, $c) }
        foreach ($m in [regex]::Matches($code, $literal)) {
            if ($m.Groups[1].Value -match '[가-힣]') { $problems += "${name}:${lineNo}: Korean string in editor code: `"$($m.Groups[1].Value)`"" }
        }
    }
}

# 2) Data text declared by the engine (translated by the editor with trData): field descriptions (last string), built-in scene kind names
foreach ($file in Get-ChildItem $engineDir -File -Include *.cpp, *.h -Recurse) {
    if ($file.FullName.StartsWith($editorDir)) { continue }
    $text = [IO.File]::ReadAllText($file.FullName, $utf8)
    foreach ($m in [regex]::Matches($text, '(?m)^\s*MEALKIT_(?:VIEW_)?FIELD(?:_FLAGS)?\(([^;]*)\);')) {
        $strings = [regex]::Matches($m.Groups[1].Value, $literal)
        if ($strings.Count -gt 0) {
            # "name|description|group=...|range=...": name, description and group are translated, the other attributes aren't text (FieldEditor.h)
            foreach ($part in (Unescape-C $strings[$strings.Count - 1].Groups[1].Value) -split '\|') {
                if ($part -match '^(range|step|unit|pick)=') { continue }
                Add-Key ($part -replace '^group=', '') $file.Name
            }
        }
    }
    foreach ($m in [regex]::Matches($text, 'makeBuiltin\(\w+,\s*' + $literal)) { Add-Key (Unescape-C $m.Groups[1].Value) $file.Name }
}

# 3) Language tables
function Format-Specs([string]$s) { return ([regex]::Matches($s, '%[-+ #0]*\d*(?:\.\d+)?[hlzjt]*[diuxXfFeEgGcsp%]') | ForEach-Object { $_.Value }) -join " " }
foreach ($table in Get-ChildItem $stringsDir -Filter *.txt) {
    $entries = New-Object System.Collections.Hashtable ([StringComparer]::Ordinal) # tr("Copy") and tr("copy") are different keys
    $lineNo = 0
    foreach ($line in [IO.File]::ReadAllLines($table.FullName, $utf8)) {
        $lineNo++
        if ($line -eq "" -or $line.StartsWith("#")) { continue }
        $eq = $line.IndexOf(" = ")
        if ($eq -lt 0) { $problems += "$($table.Name):${lineNo}: missing ' = '"; continue }
        $key = Unescape-C $line.Substring(0, $eq)
        $value = Unescape-C $line.Substring($eq + 3)
        if ($entries.ContainsKey($key)) { $problems += "$($table.Name):${lineNo}: duplicate `"$key`"" }
        $entries[$key] = $value
        if (-not $keys.Contains($key)) { $problems += "$($table.Name):${lineNo}: unused `"$key`"" }
        elseif ((Format-Specs $key) -ne (Format-Specs $value)) { $problems += "$($table.Name):${lineNo}: format differs from the original: `"$key`"" }
    }
    $missing = @($keys.Keys | Where-Object { -not $entries.ContainsKey($_) })
    foreach ($k in $missing) { $problems += "$($table.Name): missing `"$k`" ($($keys[$k]))" }
    if ($List) {
        foreach ($k in $missing) { Write-Output (($k -replace '\\', '\\' -replace "`n", '\n') + " = ") }
    }
}

if ($List) { exit 0 }
if ($problems.Count -gt 0) {
    $problems | ForEach-Object { Write-Output $_ }
    Write-Output "FAIL editor strings: $($problems.Count) problem(s), $($keys.Count) keys"
    exit 1
}
Write-Output "OK editor strings: $($keys.Count) keys"

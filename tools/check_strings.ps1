# 에디터 UI 글자 검사 (docs/CODING_MODEL.md "에디터 UI 문자열과 언어"). check.ps1이 부른다.
#   powershell -ExecutionPolicy Bypass -File tools\check_strings.ps1          문제가 있으면 종료 코드 1
#   powershell -ExecutionPolicy Bypass -File tools\check_strings.ps1 -List    빠진 번역을 "원문 = " 줄로 출력 (ko.txt에 붙여 채운다)
# - 에디터 코드의 tr()/trl()/trf() 원문, "###" 이름, 엔진이 선언한 필드 설명·씬 종류 이름(trData)이 engine/res/strings/<언어>.txt에 모두 있는가
# - 표에 있지만 어디서도 쓰지 않는 줄
# - 번역의 printf 형식(%s, %u ...)이 원문과 같은가
# - 에디터 코드에 한글 문자열이 남았는가 (그 줄에 "i18n-ignore"를 쓰면 뺀다)
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
# "##" 앞부분이 번역 키다 (Strings.cpp translate와 같다)
function Key-Of([string]$s) {
    $i = $s.IndexOf("##")
    if ($i -ge 0) { return $s.Substring(0, $i) }
    return $s
}

$keys = [ordered]@{}   # 키 → 처음 나온 곳
$problems = @()
function Add-Key([string]$key, [string]$where) {
    if ($key -ne "" -and -not $keys.Contains($key)) { $keys[$key] = $where }
}

# 1) 에디터 코드
foreach ($file in Get-ChildItem $editorDir -File -Include *.cpp, *.h -Recurse) {
    $raw = [IO.File]::ReadAllText($file.FullName, $utf8)
    $name = $file.Name
    # 주석은 보지 않는다 (줄 주석과 " // " 뒤)
    $text = [regex]::Replace($raw, '(?m)(^\s*//.*$| // .*$)', '')
    # tr("a" "b") 처럼 이어 쓴 문자열도 하나로
    foreach ($m in [regex]::Matches($text, '\b(?:tr|trl|trf)\(\s*((?:' + $literal + '\s*)+)')) {
        $joined = ([regex]::Matches($m.Groups[1].Value, $literal) | ForEach-Object { $_.Groups[1].Value }) -join ""
        Add-Key (Key-Of (Unescape-C $joined)) $name
    }
    # 창·팝업 이름 상수 ("Scene Tree###Hierarchy"). 앞부분이 printf 형식뿐이면("%s (%u)###level") 번역할 글자가 없다
    foreach ($m in [regex]::Matches($text, $literal)) {
        if ($m.Groups[1].Value.Contains("###") -and -not $m.Groups[1].Value.StartsWith("#")) {
            $k = Key-Of (Unescape-C $m.Groups[1].Value)
            if (($k -replace '%[-+ #0]*\d*(?:\.\d+)?[hlzjt]*[diuxXfFeEgGcsp%]', '') -match '[A-Za-z]') { Add-Key $k $name }
        }
    }
    # 명령 표 (EditorCommands.cpp): {"이름", 단축키, ...} 줄의 이름과 단축키 도움말 고정 줄 {"할 일", "키"}는 tr(c.name)·tr(row[n])로 번역한다
    if ($name -eq "EditorCommands.cpp") {
        foreach ($m in [regex]::Matches($text, '\{\s*' + $literal + '(?:\s*,\s*' + $literal + '\s*\})?')) {
            Add-Key (Key-Of (Unescape-C $m.Groups[1].Value)) $name
            if ($m.Groups[2].Success) { Add-Key (Key-Of (Unescape-C $m.Groups[2].Value)) $name }
        }
    }
    # 한글 문자열
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

# 2) 엔진이 선언한 데이터 글자 (에디터가 trData로 번역): 필드 설명(마지막 문자열), 기본 씬 종류 이름
foreach ($file in Get-ChildItem $engineDir -File -Include *.cpp, *.h -Recurse) {
    if ($file.FullName.StartsWith($editorDir)) { continue }
    $text = [IO.File]::ReadAllText($file.FullName, $utf8)
    foreach ($m in [regex]::Matches($text, '(?m)^\s*MEALKIT_(?:VIEW_)?FIELD(?:_FLAGS)?\(([^;]*)\);')) {
        $strings = [regex]::Matches($m.Groups[1].Value, $literal)
        if ($strings.Count -gt 0) { Add-Key (Unescape-C $strings[$strings.Count - 1].Groups[1].Value) $file.Name }
    }
    foreach ($m in [regex]::Matches($text, 'makeBuiltin\(\w+,\s*' + $literal)) { Add-Key (Unescape-C $m.Groups[1].Value) $file.Name }
}

# 3) 언어 표
function Format-Specs([string]$s) { return ([regex]::Matches($s, '%[-+ #0]*\d*(?:\.\d+)?[hlzjt]*[diuxXfFeEgGcsp%]') | ForEach-Object { $_.Value }) -join " " }
foreach ($table in Get-ChildItem $stringsDir -Filter *.txt) {
    $entries = @{}
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

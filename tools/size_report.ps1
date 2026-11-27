# Size of a shipping exe per library and module, read from the linker map the Export build writes beside it (cmake/MealKit.cmake).
# Runs on Windows PowerShell 5.1 and pwsh 7. Build the export first.
#   pwsh tools/size_report.ps1 [-Name sandbox,coop] [-Config Export] [-Project <project folder>] [-Out report.txt]
# Without -Name: every exe of the build that has a map. Map formats: MSVC /MAP and GNU ld -Map.
# Sizes are the bytes each object file puts into the exe's initialized sections; "(headers, linker tables, padding)" is the rest.
param(
    [string[]]$Name = @(),
    [string]$Config = "Export",
    [string]$Project = "",
    [string]$Out = ""
)
$ErrorActionPreference = "Stop"
$engine = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot "build_dirs.ps1")
if (-not $Project) { $Project = $engine }
$Project = (Resolve-Path $Project).Path
$buildDir = Get-BuildDir $Project $Config
$bin = Join-Path $buildDir "bin"
$invariant = [Globalization.CultureInfo]::InvariantCulture

$Name = @($Name | ForEach-Object { $_ -split "," } | Where-Object { $_ })
if (-not $Name.Count) {
    $Name = @(Get-ChildItem $bin -Filter *.map -ErrorAction SilentlyContinue | ForEach-Object { $_.BaseName } | Sort-Object)
    if (-not $Name.Count) { Write-Host "[size] no linker map in $bin (build the Export configuration first)"; exit 1 }
}

# --- Object file -> source file, from the compile database: maps only name the archive and the object file
$byTarget = @{} # "<target>|<object file name>" -> source
$byObject = @{} # "<object file name>" -> list of sources
$objectOf = @{} # source -> its object file (a source built twice: the last one, same section names)
$commands = Join-Path $buildDir "compile_commands.json"
if (-not (Test-Path $commands)) { Write-Host "[size] no $commands"; exit 1 }
foreach ($entry in (Get-Content -Raw -Encoding UTF8 $commands | ConvertFrom-Json)) {
    $output = ([string]$entry.output).Replace("\", "/")
    if (-not $output) { # CMake before 3.20: take it from the command line
        if ([string]$entry.command -match '(?:\s-o\s+|\s/Fo)"?([^"\s]+)') { $output = $Matches[1].Replace("\", "/") } else { continue }
    }
    $object = $output.Substring($output.LastIndexOf("/") + 1)
    $source = ([string]$entry.file).Replace("\", "/")
    if ($output -match 'CMakeFiles/([^/]+)\.dir/') { $byTarget["$($Matches[1])|$object"] = $source }
    if (-not $byObject.ContainsKey($object)) { $byObject[$object] = New-Object System.Collections.Generic.List[string] }
    $byObject[$object].Add($source)
    $objectOf[$source] = $output
}

$engineRoot = $engine.Replace("\", "/") + "/"
$projectRoot = $Project.Replace("\", "/") + "/"

function Get-SourceGroup([string]$source) {
    if ($source -match '/third_party/([^/]+)/') { return "library $($Matches[1])" }
    if ($source -match '/modules/([^/]+)/(src|include)/' -and $source.StartsWith($engineRoot + "modules/", [StringComparison]::OrdinalIgnoreCase)) { return "module $($Matches[1])" }
    if ($source -match '/engine/core/([^/]+)/') { return "kernel $($Matches[1])" }
    if ($source -match '/modules/([^/]+)/') { return "game module $($Matches[1])" }
    if ($source.StartsWith($projectRoot, [StringComparison]::OrdinalIgnoreCase) -or $source.StartsWith($engineRoot + "samples/", [StringComparison]::OrdinalIgnoreCase)) { return "game code" }
    return "other sources"
}

# Section names of one built object file (ELF; empty when readelf isn't there or it's an MSVC object)
$sectionCache = @{}
$readelf = (Get-Command readelf -ErrorAction SilentlyContinue | Select-Object -First 1).Source
function Get-Sections([string]$objectPath) {
    if (-not $sectionCache.ContainsKey($objectPath)) {
        $names = New-Object System.Collections.Generic.HashSet[string]
        if ($readelf -and (Test-Path $objectPath)) {
            foreach ($line in (& $readelf -SW $objectPath 2>$null)) { if ($line -match '^\s*\[\s*\d+\]\s+(\.\S+)') { [void]$names.Add($Matches[1]) } }
        }
        $sectionCache[$objectPath] = $names
    }
    return , $sectionCache[$objectPath]
}

# The outermost namespace in a section or symbol name (Itanium ".text._ZN7mealkit...", MSVC "?f@C@mealkit@@..."), "" if none
function Get-Namespace([string]$symbol) {
    if ($symbol -cmatch '_Z[A-Z]*?N[VKrO]*(\d+)') {
        $start = $symbol.IndexOf($Matches[0]) + $Matches[0].Length
        $length = [int]$Matches[1]
        if ($start + $length -le $symbol.Length) { return $symbol.Substring($start, $length) }
    }
    if ($symbol -cmatch '@([A-Za-z_]\w*)@@') { return $Matches[1] }
    return ""
}

# $archive: library name without folder and extension ("" for an object linked directly); $target: the CMake target that built a direct
# object; $symbol: tells apart objects of the same file name (the engine library holds several targets' objects, so Renderer.cpp.o is twice)
function Get-Group([string]$archive, [string]$object, [string]$target, [string]$symbol) {
    if (-not $archive -and $byTarget.ContainsKey("$target|$object")) { return Get-SourceGroup $byTarget["$target|$object"] }
    if (-not $byObject.ContainsKey($object)) { # not built here: the C/C++ runtime, system and SDK libraries
        $from = if ($archive) { $archive } else { $object }
        return "toolchain $from"
    }
    $sources = @($byObject[$object])
    if ($archive -and $sources.Count -gt 1) { # a game's own objects are linked directly, not from a library
        $built = @($sources | Where-Object { -not $_.StartsWith($engineRoot + "samples/", [StringComparison]::OrdinalIgnoreCase) })
        if ($built.Count) { $sources = $built }
    }
    $groups = @($sources | ForEach-Object { Get-SourceGroup $_ } | Sort-Object -Unique)
    if ($groups.Count -eq 1) { return $groups[0] }
    if ($symbol.StartsWith(".")) { # which of those object files has a section of this name?
        $holding = @($sources | Where-Object { (Get-Sections $objectOf[$_]).Contains($symbol) } | ForEach-Object { Get-SourceGroup $_ } | Sort-Object -Unique)
        if ($holding.Count -eq 1) { return $holding[0] }
    }
    $namespace = Get-Namespace $symbol
    if ($namespace) {
        $matching = if ($namespace -ceq "mealkit") { @($sources | Where-Object { $_ -notmatch '/third_party/' }) }
        elseif ($namespace -ceq "JPH") { @($sources | Where-Object { $_ -match '/third_party/JoltPhysics/' }) }
        else { @($sources | Where-Object { $_.Contains("/$namespace/") }) }
        $named = @($matching | ForEach-Object { Get-SourceGroup $_ } | Sort-Object -Unique)
        if ($named.Count -eq 1) { return $named[0] }
    }
    return "unresolved: $($object -replace '\.(o|obj)$', '') in $($groups -join ' or ')"
}

function Add-Size($sizes, [string]$group, [long]$bytes) {
    if ($bytes -le 0) { return }
    if ($sizes.ContainsKey($group)) { $sizes[$group] += $bytes } else { $sizes[$group] = $bytes }
}

# GNU ld: output sections start in column 0, the input sections under them are indented by one space
# (" .text.foo  0x401000  0x30 path/libx.a(y.o)", wrapped onto a second line when the section name is long)
function Read-GnuMap([string]$map, [string]$target, $sizes) {
    $inMemoryMap = $false
    $counted = $false # the current output section occupies the file
    $section = "" # the input section being listed (its name may be alone on the line before)
    foreach ($line in [IO.File]::ReadLines($map)) {
        if (-not $inMemoryMap) { if ($line.StartsWith("Linker script and memory map")) { $inMemoryMap = $true }; continue }
        if ($line.Length -eq 0) { continue }
        if ($line[0] -eq '.') {
            $output = ($line -split '\s+')[0]
            # Not in the file, or made by the linker (it lists those under the first startup object): the rest line covers them
            $counted = $output -notmatch '^\.(bss|tbss|lbss|debug|comment|symtab|strtab|shstrtab|gnu|note|interp|dynsym|dynstr|dynamic|hash|rela|relr|plt|got|eh_frame_hdr)'
            continue
        }
        if (-not $counted) { continue }
        if ($line[0] -ne ' ') { continue }
        if ($line.Length -gt 1 -and $line[1] -eq '.') { $section = ($line.Trim() -split '\s+')[0] }
        if ($line -notmatch '\s0x[0-9a-fA-F]+\s+0x([0-9a-fA-F]+)\s+(\S.*)$') { continue }
        $bytes = [Convert]::ToInt64($Matches[1], 16)
        $path = $Matches[2].Trim().Replace("\", "/")
        if ($line.TrimStart().StartsWith("*fill*")) { continue }
        if ($path -match '^(.*)\(([^()]+)\)$') {
            $archive = [IO.Path]::GetFileNameWithoutExtension($Matches[1])
            Add-Size $sizes (Get-Group $archive $Matches[2] $target $section) $bytes
        } elseif ($path -match '\.(o|obj)$') {
            $object = $path.Substring($path.LastIndexOf("/") + 1)
            $t = if ($path -match 'CMakeFiles/([^/]+)\.dir/') { $Matches[1] } else { $target }
            Add-Size $sizes (Get-Group "" $object $t $section) $bytes
        } # else: a symbol assignment or a linker-made section
    }
}

# MSVC: a section table, then symbols sorted by address ("0001:00000010  name  0000000140001010 f i lib:object.obj").
# A symbol's size is the distance to the next one in its section.
function Read-MsvcMap([string]$map, [string]$target, $sizes) {
    $sectionEnd = @{} # segment -> end offset of its initialized part
    $bss = New-Object System.Collections.Generic.List[object] # ranges that take no file space
    $symbols = New-Object System.Collections.Generic.List[object]
    foreach ($line in [IO.File]::ReadLines($map)) {
        if ($line -match '^\s*([0-9a-fA-F]{4}):([0-9a-fA-F]{8})\s+([0-9a-fA-F]+)H\s+(\S+)\s+\S+\s*$') {
            $seg = [Convert]::ToInt32($Matches[1], 16); $start = [Convert]::ToInt64($Matches[2], 16); $length = [Convert]::ToInt64($Matches[3], 16)
            if ($Matches[4] -like ".bss*") { $bss.Add([pscustomobject]@{ Seg = $seg; Start = $start; End = $start + $length }) }
            elseif (-not $sectionEnd.ContainsKey($seg) -or $sectionEnd[$seg] -lt $start + $length) { $sectionEnd[$seg] = $start + $length }
        } elseif ($line -match '^\s*([0-9a-fA-F]{4}):([0-9a-fA-F]{8})\s+(\S+)\s+[0-9a-fA-F]{8,16}\s+(?:f\s+)?(?:i\s+)?(\S.*)$') {
            $seg = [Convert]::ToInt32($Matches[1], 16)
            if ($seg -eq 0) { continue } # absolute symbols
            $symbols.Add([pscustomobject]@{ Seg = $seg; Offset = [Convert]::ToInt64($Matches[2], 16); Name = $Matches[3]; From = $Matches[4].Trim() })
        }
    }
    $sorted = @($symbols | Sort-Object Seg, Offset)
    for ($i = 0; $i -lt $sorted.Count; $i++) {
        $s = $sorted[$i]
        if (-not $sectionEnd.ContainsKey($s.Seg)) { continue }
        $end = $sectionEnd[$s.Seg]
        if ($i + 1 -lt $sorted.Count -and $sorted[$i + 1].Seg -eq $s.Seg -and $sorted[$i + 1].Offset -lt $end) { $end = $sorted[$i + 1].Offset }
        $inBss = $false
        foreach ($range in $bss) { if ($range.Seg -eq $s.Seg -and $s.Offset -ge $range.Start -and $s.Offset -lt $range.End) { $inBss = $true; break } }
        if ($inBss) { continue }
        if ($s.From -match '^([^:]+):(.+)$') { Add-Size $sizes (Get-Group $Matches[1] $Matches[2] $target $s.Name) ($end - $s.Offset) }
        elseif ($s.From -match '\.(o|obj)$') { Add-Size $sizes (Get-Group "" $s.From $target $s.Name) ($end - $s.Offset) }
        else { Add-Size $sizes "toolchain $($s.From)" ($end - $s.Offset) }
    }
}

function Format-KB([long]$bytes) { return [string]::Format($invariant, "{0,9:N1} KB", $bytes / 1024.0) }

$report = New-Object System.Collections.Generic.List[string]
$failed = $false
foreach ($exeName in $Name) {
    $exe = Join-Path $bin "$exeName.exe"
    if (-not (Test-Path $exe -PathType Leaf)) { $exe = Join-Path $bin $exeName }
    $map = Join-Path $bin "$exeName.map"
    if (-not (Test-Path $exe -PathType Leaf) -or -not (Test-Path $map)) {
        Write-Host "[size] FAIL: no $exe with $exeName.map (build the Export configuration first)"
        $failed = $true
        continue
    }
    $sizes = @{}
    $head = @(Get-Content $map -TotalCount 3)
    if ($head -match 'Preferred load address|Timestamp is' -or ($head.Count -and $head[0].Trim() -eq $exeName)) { Read-MsvcMap $map $exeName $sizes }
    else { Read-GnuMap $map $exeName $sizes }
    if (-not $sizes.Count) {
        Write-Host "[size] FAIL: $map has no sizes this script can read (MSVC /MAP or GNU ld -Map)"
        $failed = $true
        continue
    }
    $total = (Get-Item $exe).Length
    $report.Add([string]::Format($invariant, "{0}: {1:N0} bytes ({2:N2} MB)", $exeName, $total, $total / 1MB))
    $listed = 0L
    foreach ($item in ($sizes.GetEnumerator() | Sort-Object @{ Expression = "Value"; Descending = $true }, Name)) {
        $report.Add("  $(Format-KB $item.Value)  $($item.Name)")
        $listed += $item.Value
    }
    $report.Add("  $(Format-KB ($total - $listed))  (headers, linker tables, padding)")
    $report.Add("")
}
$report | ForEach-Object { Write-Host $_ }
if ($Out) {
    $folder = Split-Path -Parent $Out
    if ($folder) { New-Item -ItemType Directory -Force $folder | Out-Null }
    $report | Out-File -Encoding utf8 $Out
}
if ($failed) { exit 1 }
exit 0

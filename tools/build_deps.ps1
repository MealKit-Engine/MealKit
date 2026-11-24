# Header-dependency safety for the build scripts (dot-source: . "$PSScriptRoot/build_deps.ps1"). Used by tools\build.ps1, so every caller is
# safe (an agent's shell, a plain terminal, CI), and by the workspace wrapper .claude\scripts\mk-build.ps1 for the repositories that have no build.ps1.
#
# Ninja records header dependencies by matching cl.exe's /showIncludes lines against the msvc_deps_prefix stored in rules.ninja. On a
# localized Windows that prefix is in the console's code page, so a console with another code page (UTF-8) makes every line miss: the object
# gets "#deps 0" and later header edits don't rebuild it (stale objects, crashes that look like real bugs). clang and gcc write dependency
# files (-MD), so none of this applies on Linux/macOS except the "#deps 0" check, which is cheap and kept.
#   Set-BuildCodePage     Windows: switch the console to the OEM code page (the one CMake detects the prefix in) for this process
#   Restore-BuildCodePage puts the previous code page back
#   Test-DepsPrefix       before a build: does the compiler's output start with the stored prefix?
#   Get-ZeroDepObjects    after a build: objects that lost their dependencies although their source includes a project header

$MealKitDepsOnWindows = [Environment]::OSVersion.Platform -eq [PlatformID]::Win32NT
$script:MealKitSavedCodePage = $null

function Set-BuildCodePage {
    if (-not $MealKitDepsOnWindows) { return }
    $oem = [Globalization.CultureInfo]::CurrentCulture.TextInfo.OEMCodePage
    $current = ((& chcp.com) -replace '[^\d]', '')
    if ($current -and $current -ne "$oem") {
        $script:MealKitSavedCodePage = $current
        & chcp.com $oem | Out-Null
    }
}

function Restore-BuildCodePage {
    if ($MealKitDepsOnWindows -and $script:MealKitSavedCodePage) {
        & chcp.com $script:MealKitSavedCodePage | Out-Null
        $script:MealKitSavedCodePage = $null
    }
}

function Get-CacheValue([string]$buildDir, [string]$key) {
    $cache = Join-Path $buildDir "CMakeCache.txt"
    if (-not (Test-Path $cache)) { return $null }
    $line = Select-String -Path $cache -Pattern "^$key(:[A-Z]+)?=" | Select-Object -First 1
    if (-not $line) { return $null }
    return $line.Line.Substring($line.Line.IndexOf("=") + 1)
}

# Zero is normal for sources that use a precompiled header (/Yu) or include only system headers (ninja leaves those out) and for .rc resources.
function Get-ZeroDepObjects([string]$buildDir) {
    $ninja = Get-CacheValue $buildDir "CMAKE_MAKE_PROGRAM"
    if (-not $ninja -or -not (Test-Path $ninja)) { return @() }
    if (-not (Test-Path (Join-Path $buildDir ".ninja_deps"))) { return @() }
    $zero = @{}
    foreach ($line in (& $ninja -C $buildDir -t deps)) {
        if ($line -match '^(.+\.(obj|o)): #deps 0\b') { $zero[$Matches[1].Replace('\', '/')] = $true }
    }
    if ($zero.Count -eq 0) { return @() }

    $list = @()
    $object = $null
    $source = $null
    foreach ($line in [IO.File]::ReadLines((Join-Path $buildDir "build.ninja"))) {
        if ($line -match '^build (.+?\.(?:obj|o)): \S+ (\S+)') {
            $object = $Matches[1].Replace('$:', ':').Replace('$ ', ' ').Replace('\', '/')
            $source = $Matches[2].Replace('$:', ':')
            if (-not $zero.ContainsKey($object)) { $object = $null }
        } elseif ($object -and $line -match '^\s+FLAGS = ') {
            $path = if ([IO.Path]::IsPathRooted($source)) { $source } else { Join-Path $buildDir $source }
            $usesPch = $line -match '[/-]Yu'
            if (-not $usesPch -and (Test-Path $path) -and (Select-String -Path $path -Pattern '^\s*#\s*include\s*"' -Quiet)) {
                $list += $object
            }
            $object = $null
        }
    }
    return $list
}

# Compares real /showIncludes output with the prefix ninja was configured with (Windows, cl.exe only).
function Test-DepsPrefix([string]$buildDir) {
    if (-not $MealKitDepsOnWindows) { return $true } # clang/gcc: dependency files, no localized prefix
    $rules = Join-Path $buildDir "CMakeFiles\rules.ninja"
    if (-not (Test-Path $rules)) { return $true } # not configured yet: CMake detects it in the code page Set-BuildCodePage set
    $compiler = Get-CacheValue $buildDir "CMAKE_CXX_COMPILER"
    if (-not $compiler -or $compiler -notmatch '[\\/]cl\.exe$') { return $true } # clang-cl: fixed English prefix
    $latin1 = [Text.Encoding]::GetEncoding(28591)
    $match = [regex]::Match($latin1.GetString([IO.File]::ReadAllBytes($rules)), 'msvc_deps_prefix = (.*?)\r?\n')
    if (-not $match.Success) { return $true }

    $probe = Join-Path $env:TEMP ("mk-build-probe-" + [Guid]::NewGuid().ToString("N"))
    New-Item -ItemType Directory -Path $probe | Out-Null
    try {
        Set-Content (Join-Path $probe "p.h") "#pragma once"
        Set-Content (Join-Path $probe "p.cpp") '#include "p.h"'
        $output = Join-Path $probe "out.txt"
        $oem = [Globalization.CultureInfo]::CurrentCulture.TextInfo.OEMCodePage
        # cl is already on PATH when the build script ran vcvars64 (or the caller did); otherwise find it
        $vcvars = ""
        if (-not $env:VSCMD_VER) {
            $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
            $vsDir = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
            $vcvars = "call `"$(Join-Path $vsDir 'VC\Auxiliary\Build\vcvars64.bat')`" >nul 2>nul & "
        }
        cmd /c "chcp $oem >nul & $vcvars cd /d `"$probe`" & cl /nologo /showIncludes /Zs /TP p.cpp > `"$output`" 2>&1"
        $text = $latin1.GetString([IO.File]::ReadAllBytes($output))
        $line = ($text -split "`r?`n" | Where-Object { $_ -match 'p\.h' } | Select-Object -First 1)
        return ($line -and $line.StartsWith($match.Groups[1].Value))
    } finally {
        Remove-Item -Recurse -Force $probe -ErrorAction SilentlyContinue
    }
}

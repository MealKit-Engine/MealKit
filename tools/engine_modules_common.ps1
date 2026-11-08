# Where engine modules live (docs/ENGINE_MODULES.md "Layout"). Dot-sourced by the check scripts so the locations are written once.
#   . "$PSScriptRoot\engine_modules_common.ps1"
#   Get-EngineModules $root       -> objects with Name, Dir (module root), Kernel (bool)
#   Get-EngineModuleFiles $root   -> *.h/*.cpp of every module, each with .Module (name). A module's editor/ folder (its editor panels,
#                                    Editor builds only) counts as the editor module, which requires that module
#   Resolve-EngineHeader $root "core/Types.h" -> full path of include/mealkit/core/Types.h, or $null
# Kernel modules are engine/core/<name>/; the rest are modules/<name>/ (vendored copies of the module repos after step 9).
function Get-EngineModules([string]$root) {
    $out = @()
    foreach ($base in @((Join-Path $root "engine\core"), (Join-Path $root "modules"))) {
        if (-not (Test-Path $base)) { continue }
        foreach ($d in Get-ChildItem -Path $base -Directory | Sort-Object Name) {
            if (-not (Test-Path (Join-Path $d.FullName "module.txt"))) { continue }
            $out += [pscustomobject]@{ Name = $d.Name; Dir = $d.FullName; Kernel = ($base -like "*engine\core") }
        }
    }
    return $out
}

function Get-EngineModuleFiles([string]$root) {
    $files = @()
    foreach ($m in Get-EngineModules $root) {
        $editorDir = (Join-Path $m.Dir "editor") + "\"
        foreach ($f in Get-ChildItem -Path $m.Dir -Recurse -Include *.cpp, *.h) {
            $owner = if ($f.FullName.StartsWith($editorDir, [StringComparison]::OrdinalIgnoreCase)) { "editor" } else { $m.Name }
            $f | Add-Member -NotePropertyName Module -NotePropertyValue $owner -Force
            $files += $f
        }
    }
    return $files
}

function Resolve-EngineHeader([string]$root, [string]$path) {
    $module = $path.Split('/')[0]
    foreach ($m in Get-EngineModules $root) {
        if ($m.Name -ne $module) { continue }
        $full = Join-Path $m.Dir ("include\mealkit\" + $path.Replace('/', '\'))
        if (Test-Path $full) { return $full }
    }
    return $null
}

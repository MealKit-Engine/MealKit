# GameContext access check (docs/CODING_MODEL.md "How to access engine features", docs/DECISIONS.md 2026-11-25). Called by check.ps1.
#   pwsh -NoProfile -File tools/check_context_access.ps1 [-Path <folder>]     exit code 1 on problems
# Game code reaches a service that GameContext already holds through the context (ctx.ui), never through the registry
# (services::get<ui::System>() / ctx.get<ui::System>()): one answer per problem, and no hidden global next to the shadow simulation.
# get<T>() stays for services outside GameContext (online and other modules a game adds to its preset).
# Targets: samples\<name>\{sim,view,app,modules}, templates\, and the folders given with -Path (a game project's source\ or a module checkout).
param([string[]]$Path = @())
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
# Services GameContext holds (keep in sync with GameContext.h; a member added there is a static_assert failure that points here)
$held = 'Window', 'render::Renderer', 'Renderer', 'asset::ModelCache', 'ModelCache', 'session::Session', 'Session',
        'MatchService', 'game::MatchService', 'audio::Engine', 'feedback::Player', 'input::Actions', 'Actions',
        'loc::Strings', 'Strings', 'ui::System', 'settings::Service'
$alt = ($held | ForEach-Object { [regex]::Escape($_) }) -join '|'
$rx = "\bget\s*<\s*(?:const\s+)?(?:mealkit::)?(?:$alt)\s*>\s*\("

$dirs = @()
foreach ($sample in Get-ChildItem -Path (Join-Path $root "samples") -Directory) {
    foreach ($sub in "sim", "view", "app", "modules") {
        $d = Join-Path $sample.FullName $sub
        if (Test-Path $d) { $dirs += $d }
    }
}
$dirs += Join-Path $root "templates"
foreach ($p in $Path) { $dirs += (Resolve-Path $p).Path }

$problems = @(); $count = 0
foreach ($dir in $dirs) {
    foreach ($f in Get-ChildItem -Path $dir -Recurse -Include *.h, *.hpp, *.cpp, *.inl) {
        if ($f.FullName -match '[\\/](build|export|third_party)[\\/]') { continue }
        $count++
        $rel = if ($f.FullName.StartsWith($root)) { $f.FullName.Substring($root.Length + 1) } else { $f.FullName }
        $n = 0
        foreach ($line in [IO.File]::ReadAllLines($f.FullName)) {
            $n++
            $code = $line -replace '//.*$', ''
            if ($code -match $rx) { $problems += "${rel}:${n}: get<T>() for a service GameContext already holds; use the context member (docs/CODING_MODEL.md)" }
        }
    }
}
if ($problems.Count -gt 0) {
    $problems | ForEach-Object { Write-Output "error: $_" }
    Write-Output "FAIL context access: $($problems.Count) problem(s)"
    exit 1
}
Write-Output "OK context access: $count game code file(s) use GameContext members, not get<T>(), for held services"
exit 0

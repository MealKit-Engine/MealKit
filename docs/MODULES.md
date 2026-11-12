# Source modules

How source modules work: what a module is, how it enters a game's build, where modules live and how they're found. Decided in `docs/DECISIONS.md` 2026-10-28 "Source modules" (direction from 2026-10-16/2026-10-21); built in stage 4 ①~⑧, details in `docs/CHANGELOG.md`. The modules still to write are in `docs/ROADMAP.md` "Modules ⑨". The user-facing pages are the docs repo's "Manual > Modules" and "Working on the engine > Writing modules".

## Summary

Since 2026-11-09 engine modules live in the same `modules/<name>/` with the same `module.txt` (plus `type engine`) and the same `tools\module.ps1` (`docs/ENGINE_MODULES.md` "Module selection and versions"). This document is about source modules; where engine modules differ, that document says so.

A module is **a source folder copied into the project's `modules/<name>/`**. The build gathers that folder's `sim/` and `view/` like game code and includes them in SimCheck and the simulation version hash. Assets are read via `modules/<name>/...` paths.
Game code puts module state in as a `WorldState` field and calls module functions from `World::step()`. The engine doesn't call them automatically.
**One module = one git repo**, official or not. When a one-line info file per module is placed in the repo **`MealKit-Modules` (registry)**, CI verifies it and builds `index.json` and a search site. The editor, Manager, and `tools\module.ps1` find modules by name in that list and add them.
There are no modules in the engine repo. Only copies of modules used by the reference samples exist, like in any other game.

## Why this approach

| Alternative | Why not used |
|---|---|
| Built-in engine features | The engine gets heavier and one design is forced on every game |
| Official modules in the engine repo's `modules/` (draft 1) | Only added modules go into the shipping exe, but unused features come along in the engine repo and distributions, and they look like "the engine's chosen answer", getting different treatment from others' modules. Official module releases get tied to engine releases |
| Prefabs only | They hold only data (placement, values) and can't carry `sim/` logic |
| Binary plugins (DLLs) | The shipping exe is a static single file, and `sim/` must pass SimCheck |
| Package managers (dependency resolution at install, vcpkg/npm style) | Doesn't fit the offline, version-pinned, source-included principles, and games couldn't modify module code |
| Upload server (asset store style: accounts, file uploads, review) | Costs accounts, storage, and operations, and nobody would use it yet. Revisit when the registry isn't enough (deferred table) |

Copying source has benefits. Cloning one project repo is enough to build, and games freely modify modules. SimCheck, hashes, snapshots, and desync diagnosis apply as is.
The cost is that modified files must be merged by hand on update (`.new`).

## Module repo layout

The module repo's root is the module folder. It repeats the project folder layout (`docs/CODING_MODEL.md` "Project folder layout"), and `tools\check_layout.ps1` checks it with the same rules.

```
module-cooldown/             (repo = modules/cooldown/ once copied into a project)
  module.txt          module info (below)
  LICENSE
  sim/                state MEALKIT_STRUCT + system functions. Checked by SimCheck
  view/               drawing, filling view models, feedback
  assets/<kind>/      .ui, .table, prefabs, sounds, etc. Kind folder rules are the same as the project's assets/
  art/                Penpot sources etc. (excluded from shipping and hash)
  docs/               index.md (English source, single-page template per DECISIONS 2026-10-21)
  tests/              module unit tests (built only by module CI, ignored by game builds)
  .github/workflows/  one line calling the registry's shared workflow (below, "Module CI")
```

- Only the module structure is copied: `module.txt`, `sim/`, `view/`, `assets/`, `art/`, `docs/`, and root `LICENSE*`/`README*` (excluding `*.new`). Build folders, `tests/`, `.git`, `.github/` don't go. `module.files` (the install record) is added at that time. `module.files` has a first line `source folder <path>` or `source git <url> <tag|-> <commit> [modules/<name>]`, followed by `<sha256>  <module-relative path>` lines. Hashes are line-ending-independent for text files and bytewise otherwise.
- There's no `app/`. Screen flow and sessions belong to the game. One module is one feature, and big features are split into several small modules with dependencies.
- **There are no official modules** (user decision 2026-10-28). The first modules also live in author KingProtea99's repos (`github.com/KingProtea99/module-<name>`, MIT) and are registered exactly like others' modules. `index.json`, the site, and tools have no official mark. The repo name `module-<name>` is only recommended.
- New modules are made from the registry's `template/` with `python tools/new_module.py <name> <folder> --author <name>` (no separate `module-template` repo. The template, shared CI, and registration checks must be in one repo to be fixed together).

### `module.txt`

```
@format module 1
name cooldown
version 0.1.0
engine 0.8
title Cooldowns
summary TickTimer bundles for skill cooldowns, with a cooldown icon .ui
tags gameplay timer ui
icon timer
requires
```

`type` is `source` when absent; `type engine` marks an engine module. In `requires`, a name may be an engine module of the build (a minimap needs `render`), with an optional minimum version after it.

| Key | Meaning |
|---|---|
| `name` | The folder name, C++ namespace, and asset path prefix. Lowercase letters, digits, `_` only. Whoever registers first in the registry gets it |
| `version` | The module's semver. Must match the repo tag `v<version>` (checked by registry CI) |
| `engine` | The compatible engine MINOR. During 0.x, MINOR is the API boundary |
| `requires` | Dependency modules as `name minVersion`, space-separated |
| `title`/`summary`/`tags` | Used for listing and search |
| `icon` | Optional. One keyword for the picture in the editor [Modules] window and MealKit Manager (below). Without it, the first tag that is a keyword, then the name, then a puzzle piece |

Icon keywords (each program draws its own picture for them; unknown keywords fall back as if absent): `gameplay`, `timer`, `combat`, `health`, `ai`, `map`, `save`, `ui`, `audio`, `render`, `camera`, `physics`, `net`, `online`, `input`, `data`, `asset`, `scene`, `debug`, `tool`, `settings`, `loc`, `feedback`, `live`, `session`, `sim`, `game`, `editor`.
They include the engine module names, so engine modules get their picture without an `icon` line.

## How modules enter the build

`mealkit_add_game()` in `cmake/MealKit.cmake` gathers `modules/*/` the same way as `source/`. The game's `CMakeLists.txt` doesn't change. Modules have no CMake files, so adding a module doesn't run module code at configure time.

| What | Now (game `source/`) | Added for modules |
|---|---|---|
| Source gathering | `source/{sim,view,app}/**` | `modules/*/{sim,view}/**` |
| Include paths | `source/` → `#include "sim/World.h"` | `modules/` → `#include "cooldown/sim/Cooldown.h"` |
| SimCheck | `sim/` files | Module `sim/` is checked too. A module's `sim/` includes only the engine core, the same module's `sim/`, and dependency modules' `sim/`. Including the game's `source/` fails (modules don't know the game) |
| Simulation version hash | game `sim/` + engine core | Module `sim/` is included too. Peers with different module sim code are refused connection (the current rule as is) |
| Headless target | `sim/` + `Headless.cpp` | Module `sim/` is included too |
| Hot reload | game DLL | Goes into the same DLL, so as is |

- **namespace**: module code is in `namespace <name>` (e.g. `cooldown::`). The registry gives out each name only once, so modules don't collide.
- **"Takes only its own state"**: module system function arguments are only the module state reference, the tick number, the module tuning, and engine core types (`Pool`, `Id`, `PhysicsWorld&` queries, etc.). Anything to tell the game is returned via return values or a result array inside the module state (no callback registration, `docs/CODING_MODEL.md` "Notifying within the simulation").

### How a game uses a module

```cpp
// source/sim/World.h
#include "cooldown/sim/Cooldown.h"
MEALKIT_STRUCT(WorldState) {
    ...
    MEALKIT_FIELD(cooldown::Cooldowns, cooldowns, {}, "Cooldowns");   // automatically goes into the hash, snapshots, and [State] panel
};

// source/sim/Tuning.h
MEALKIT_STRUCT(Tuning) {
    ...
    MEALKIT_FIELD(cooldown::Tuning, cooldown, {}, "Cooldown module");  // goes into game.tuning like "cooldown.slots 8"
};

// source/sim/World.cpp — the game decides the call order
void World::step(...) {
    ...
    cooldown::step(state.cooldowns, tick);
    for (const auto& ready : cooldown::readyThisTick(state.cooldowns)) onCooldownReady(ready);
}
```

- **Tuning**: a module only provides its own `Tuning` struct with defaults. It's a nested field of the game's `Tuning`, so there's no separate file. `game.tuning`, the [Tuning] panel, and the content hash use it as now (Reflect already supports nested `MEALKIT_STRUCT`s and `a.b` paths). View values go in `view.tuning` the same way.
- **Tables**: if a module needs a definition data table, it points to the example table in the module folder. e.g. `MEALKIT_TABLE(ItemDef, "modules/inventory/tables/items.table", 0)`. If the game edits that table, it becomes a modified file.
- **No automatic registration or calls.** The order must be visible in game code as is for determinism debugging. The glue code is put verbatim in the module docs' "How to add".

## Asset paths

Export also copies `modules/*/assets/` to the shipping folder `modules/<name>/assets/` (`cmake/MealKitExport.cmake`). Module components are put into game screens with `use <name> modules/<module>/ui/x.ui` in `.ui`.

Paths starting with `modules/<name>/<kind>/...` resolve under that module's `assets/`. e.g. `modules/health/ui/healthbar.ui` → `<project>/modules/health/assets/ui/healthbar.ui`.
Only kind folders can be directly under `assets/`, so this doesn't collide with project assets. Paths are resolved in only one place, `assetFilePath` in `platform/Paths.h` (file lookup is `findAssetFile`) (it's the bottom layer, so tables, UI, sound, models, and the editor can all use it). The content hash, asset browser, and asset cooking use the same function.
If a game wants to change a module's `.ui` a lot, it copies it into the project's `assets/ui/` and uses its own path. That leaves the module file unmodified, keeping updates easy.

## The registry `MealKit-Modules`

This is where modules are gathered. It's made without a server, as **a git repo + CI + a static site**. Names have a single source of truth here (the same approach as crates.io-index or a Homebrew tap).

```
MealKit-Modules/
  entries/<name>.txt       one file per module (added by people via PR)
  index.json               full list built by CI (what the editor, Manager, and module.ps1 download)
  site/                    search site built by CI (GitHub Pages)
  template/                new module template
  .github/workflows/       PR checks, daily re-reads, the shared workflow module repos call
```

### Registration file `entries/<name>.txt`

```
@format module-entry 1
name cooldown
repo https://github.com/KingProtea99/module-cooldown
author KingProtea99
```

- Registration files contain only the name and repo. Version, summary, dependencies, and engine are read by CI from the module repo's tags and `module.txt`. This keeps information from living in two places.
- There's no official mark. All modules get the same treatment.

### What CI does

| When | What it does |
|---|---|
| Registration PR | Checks that the name is free and follows the rules. Reads `v*` tags from the repo and checks each tag's `module.txt` (name/version match), `LICENSE`, and `docs/index.md`. Puts the latest tag **into an empty project and builds with the engine version in `module.txt`**, and runs SimCheck and the module's `tests/`. Failure blocks the PR |
| Daily | Reads new tags of all registered modules and rebuilds `index.json` and the site. New tags are checked as above, and also built with the latest engine release, recording states like "build fails on engine 0.9" |
| Before an engine release | Engine repo CI uses the registry's shared workflow to **build all registered modules against the new engine**. If an API change breaks modules, it's known before release (this replaces draft 1's reason for keeping them in the engine repo) |

### `index.json` (one module example)

```json
{ "name": "cooldown", "repo": "https://github.com/KingProtea99/module-cooldown",
  "title": "Cooldowns", "summary": "...", "tags": ["gameplay","timer","ui"], "license": "MIT", "author": "KingProtea99",
  "versions": [ { "version": "0.1.0", "tag": "v0.1.0", "commit": "3f2a…", "engine": "0.8", "requires": [],
                  "checks": { "0.8": "pass", "0.9": "fail" } } ] }
```

`commit` is recorded, so if someone moves a tag, the commit differs on download and it's noticed.

### Three places to browse (same `index.json`)

| Where | What |
|---|---|
| Site (`mealkit-engine.github.io/MealKit-Modules`) | Search, tags, engine version filter. A page per module rendering `docs/index.md`, version/build status, repo link, a one-line add command |
| MealKit Manager [Modules] tab | The same list and search as the site. Adds to the chosen project (calls that project engine's `tools\module.ps1`). The downloaded list is cached in `%LOCALAPPDATA%\MealKit\module-index.json`, and the cache is shown when offline (`module.ps1` uses the same cache. The list URL is `raw.githubusercontent.com/MealKit-Engine/MealKit-Modules/main/index.json`, changed with `-Index`/`MEALKIT_MODULE_INDEX`) |
| Editor [File] > [Modules...] (`editor/ModulesPanel`) | An installed module table (name, version and new version, modified file/`.new` counts, source, [Update]/[Remove]/[Docs]) and the registry list ([Add]). Not changed during play; changes rebuild game code |

The editor and Manager get installed modules (`installed` lines) and the registry (`module` lines, or `error` lines if download fails) with one `module.ps1 search -Tsv -Project <folder>`. The line shape is known in three places together: `module.ps1`, the editor's `ModulesPanel.cpp`, and Manager's `parseModuleList` in `Catalog.cpp`.

When adding a registry module for the first time, it warns once (the same for any module). "This code runs inside the editor and the game. Registry CI only checks the build and doesn't review the code."

## Add, update, remove (`tools\module.ps1`)

The rules live in one PowerShell tool (`tools\module.ps1`, reusing `project_common.ps1`). The editor and Manager call this tool (the same relationship as `upgrade_project.ps1` now).

```
tools\module.ps1 search  [<query>] [-Tag <tag>]                    registry list (those matching the project engine version first)
tools\module.ps1 list    -Project <folder>                         installed modules, versions, modified file counts, new versions available
tools\module.ps1 add     -Project <folder> <name[@version] | gitUrl#tag | folder>
tools\module.ps1 update  -Project <folder> <name> [-To <version>] [-DryRun] [-Force]
tools\module.ps1 update  -Project <folder> -All [-DryRun]          every registry/engine module with a newer version for the engine MINOR
tools\module.ps1 remove  -Project <folder> <name>
tools\module.ps1 resolve -Project <folder> <name> -File <path> -Use new|mine   settle a <file>.new left by update (the record takes the .new's hash)
tools\module.ps1 versions <name> [-Project <folder>] [-Tsv]          registry versions, or the engine module repo's v* tags
```

| Action | What it does |
|---|---|
| add (name) | Picks the newest version in `index.json` matching the project engine MINOR. Downloads with `git clone --depth 1 --branch <tag>` into `%LOCALAPPDATA%\MealKit\module-cache\` and checks the commit matches the list. Shows a list of modules missing from `requires` (`-WithRequires` adds them too). After copying, writes per-file sha256 and `source registry cooldown 0.1.0 <commit> <repo> <tag>` to `module.files` (`update` uses this line to find it in the registry again, and `-To <version>` picks a version), and opens the module docs' "How to add" |
| add (git URL/folder) | Modules not in the registry (in development, in-house). Same approach, recorded as `source git …`/`source folder …` |
| update | The upgrade tool rules as is. Unmodified files (hash matches the record) are replaced with new ones, and modified files are left with `<file>.new` written. Files removed in the new version are deleted if unmodified, or left with a notice if modified |
| remove | Refuses if another module `requires` it. If the game's `source/` has lines including `"<name>/`, it shows those lines and refuses (`-Force` to proceed anyway). Deletes the folder (git handles undo) |

- **The installed list is the `modules/` folder itself.** There's no separate list in `mealkit.project`. The list and folders can't drift apart.
- Modules are committed to the project repo. Teammates and CI build without downloading anything. The network is used only for `search`, `add`, `update`.
- The reference sample `samples/sandbox` also commits a `modules/cooldown/` copy just like any other game. This is the only source module in the engine repo.

## Module CI (per module repo)

Module unit tests are `tests/CMakeLists.txt` (includes the engine's `cmake/MealKit.cmake`, links `mealkit_core`, and creates `#include "<name>/..."` forwarding headers in the build folder) + `tests/*.cpp` (`main`). Built standalone with `cmake -S tests -B build/tests -G Ninja -DMEALKIT_ROOT=<engine> -DMEALKIT_HEADLESS_ONLY=ON`. The shared workflow runs this.

The template contains one workflow line. It calls the registry's shared workflow (`uses: MealKit-Engine/MealKit-Modules/.github/workflows/module.yml@v1`).
The shared workflow fetches the newest engine tag `v<MAJOR.MINOR>.*` matching the engine version in `module.txt` (`main` if none), and runs the registry's `tools/check_module.ps1` on Windows and Linux: checks `module.txt`, `LICENSE`, `docs/index.md`, builds and runs `tests/`, places it in `modules/<name>/` and runs SimCheck. The same script checks locally too. Building a whole game in an empty project and cross-platform hash comparison don't exist yet.
All modules use the same checks, so even with many repos, CI is fixed in one place.

## Docs

Module docs are one page, `docs/index.md`, in the module repo. The template stays: Overview / How to add (glue code) / Usage / How it works / Version history. It's copied into projects too, so the docs for the version in use stay alongside.

- **Where to read them**: the registry site renders all module docs in the same shape. The same regardless of author.
- The docs repo (MealKit-docs) "Game developer guide > Modules" has only one how-to page and a link to the registry site. "Engine developer guide > Writing modules" gets this document's rules (folders, `module.txt`, include rules, namespace, how to register).
- This changes DECISIONS 2026-10-21's "the docs build gathers the engine's `modules/*/docs`". Korean goes in the module repo's `docs/index.ko.md` (optional), and the site lets you pick the language.

## Decisions (2026-10-28)

1. One module = one repo (official modules alone aren't gathered into one repo)
2. The registry is a git repo + CI + GitHub Pages (not just topic search, not an upload server — the server is in the deferred table)
3. Registration PRs are merged by a person after CI passes
4. The installed list is the `modules/` folder itself (no list in `mealkit.project`)
5. Asset path prefix `modules/<name>/...`, namespace is the module name
6. Module docs on the registry site, MealKit-docs has one guide page and links. Korean in `index.ko.md`

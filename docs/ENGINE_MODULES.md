# Engine module split

**Status: decided 2026-11-08 with the user (`docs/DECISIONS.md` 2026-11-08 "Engine module split"), being built (ROADMAP "Engine module split").** The engine itself keeps only minimal features such as memory management, and the renderer, physics, networking and the other subsystems become modules, **each in its own repo**.
This reverses `docs/DECISIONS.md` 2026-10-19 ("no toggleable build modules, a folder layer order instead") and 2026-10-26 "When splitting out subsystems" ②, and changes the absolute rule "always multiplayer-capable" to "multiplayer by default".
When done, rules move to `docs/CODING_MODEL.md` and the CLAUDE.md files, implementation to `docs/CHANGELOG.md`, and this document is deleted (the same life as `docs/MODULES.md`).

## Summary

- The engine becomes a **kernel** (`core`): memory, types and math, determinism tools, reflection, text formats, logging and profiling, the platform basics (files, time, threads), jobs, and the module mechanism itself. It knows no graphics, physics, network or sound.
- Everything else is an **engine module**: `rhi`, `render`, `physics`, `net`, `sim`, `audio`, `ui`, `input`, `game`, `editor`, ... Each module has a manifest that declares what it requires; the folder layer order becomes a dependency graph read from those manifests.
- **One module = one repo** (`module-<name>`), from the first public release. The `MealKit` repo keeps the kernel, the build system, project tools, templates, reference samples, and the **lock file** that pins which module versions make up an engine version.
- Modules are **linked statically, chosen at build time**: a project picks **any combination** (presets are named starting points) and may pin **each module's version** on its own, like source modules (decided 2026-11-08, second row; "Module selection and versions" below). Not runtime plugins: the shipping exe stays a single static file, calls stay inlinable, and peers can't mix different module builds.
- **Multiplayer is a module too.** `net` is in the default preset, so new games are multiplayer-capable as now, but a game may leave it out. The determinism rules stay for every game (replays, rewind, scenario tests, and adding `net` later all depend on them).

## What changes from the earlier decisions

The 2026-10-19 and 2026-10-26 rows kept the subsystems inside one library. Their reasons, and how this design answers each:

| Earlier reason | Answer here |
|---|---|
| Exe budget is fine (2.4 MB, later 3.7 MB of 5 MB), so nothing needs removing | Still true. The goal is a small, stable kernel and replaceable parts. Size per preset is measured anyway |
| The editor shares one `mealkit.dll` across all projects | Kept: the editor links every module in the lock file. Per-module editor DLLs aren't part of this |
| `GameContext` holds services by reference; optional services would need "when absent" handling | Absence is a **build error, not a runtime check**: a module's headers are on the include path only when the project enables it. `GameContext` keeps its reference members for the modules the `game` framework requires ("Services") |
| Combinations to check | Engine CI checks the **presets** with the locked versions. Any other list or version is the game's own responsibility, like game libraries; configuring checks `requires` versions and missing requirements, and the simulation version keeps different builds apart |
| Runtime plugins cost indirect calls, LTO, the dynamic CRT, determinism risk, ABI work | Not used. Static linking keeps all of that as today; the only virtual interfaces are the ones that already exist (RHI backends, online backends) plus the session interface for optional `net` |
| Separate repos before interfaces settle cost cross-repo syncing (why UI 2 waited for v1.0) | Accepted as the price. Reduced by the lock file, local checkout mode and shared CI below |

## The kernel (`core`)

What stays in the engine itself. Everything here runs with no window, no GPU, no network, on every platform, and is allowed in `sim/`.

| Area | Contents | From |
|---|---|---|
| Memory | allocators, arenas, tags and budgets (below) | new; `debug/AllocCount` moves here |
| Types and math | `Types.h`, `Math.h`, `DetMath.h`, `Random.h` (`Pcg32`), `Hash.h` | `core/` |
| Containers for state | `Pool<T>`, `Id<T>`, `TickEvents`, `TickTimer` | `sim/` (generic, no game logic) |
| Data description | `Reflect.h` (`MEALKIT_STRUCT`/`FIELD`/`ENUM`), `TextFormat.h`, `Version.h` | `core/` |
| Diagnostics | `Log.h`, `Profile.h` (Tracy), `MEALKIT_ASSERT`, debug draw queue (empty inline in shipping) | `core/`, `debug/DebugDraw.h` |
| Platform basics | files, paths (`assetFilePath`), packs, time, threads, processes, dynamic libraries | `platform/` minus window, input, gamepad, dialogs, ImGui |
| Jobs | `parallelFor`, `run` → `Task` | `jobs/` |
| Modules | manifests, init/shutdown in dependency order, service registry | new |
| Compression, zip, contract | `Compress`, `Zip`, mkcontract glue | `core/` |

Not in the kernel: the window (a `window` module, because headless has none), the frame loop (owned by the `game` module, so the call order stays visible in one place), and anything that knows about matches.

### Memory management

Today the engine uses the global heap everywhere except `sim/` (which bans allocation and uses `Pool` capacity set at match start). The kernel adds one answer for where memory comes from and who used it:

```cpp
// core/Memory.h
namespace mealkit::mem {
// One allocator interface: std::pmr::memory_resource, so std::pmr containers work with every allocator below.
using Resource = std::pmr::memory_resource;

Resource& heap(Tag tag);          // the global heap, counted per tag
class Arena;                      // linear, reset all at once; grows by pages from the platform
class FixedPool;                  // fixed-size blocks (bodies, voices, UI nodes)

Arena& frameArena();              // reset at the end of each frame (draw lists, UI layout scratch)
Arena& matchArena();              // reset when a match stops (Pool storage, scene copies, tables read for the match)

struct TagStats { u64 bytes, peak, allocations; };
TagStats stats(Tag tag);          // shown in FrameStats and the editor [Debug] > [Memory] page
}
```

- **Tags are modules.** Each module declares its tag in its manifest, and its allocations through `mem::heap(tag)` or its arenas are counted under it. Projects may set per-tag budgets in `mealkit.project`, warned (not blocked) the same way as asset and GPU budgets.
- **Pool storage** is counted under the tag "sim" with the stateless `mem::Allocator`, not taken from the match arena: Pools are copied (shadow simulation, rewind, snapshots) and grow mid-match, which a reset-all arena would break. The match arena is for data that lives exactly one match and isn't copied.
- **The frame arena** replaces per-frame `std::vector` growth found by the frame allocation check (ROADMAP optimization 9), only where measurement shows allocations.
- Counting the global `operator new` stays a dev game exe feature (as `AllocCount` today). The editor DLL counts only tagged allocations.
- Third-party libraries get their allocator hooks pointed at a tag where they offer one. Jolt's hooks are process-wide and shared by the job pool and physics, so they're installed once (`jobs/JoltAllocator.cpp`, tag "jolt"); miniaudio and meshoptimizer follow.

## Engine modules

### Module layout (= the root of its repo)

```
module-render/               repo root = the module
  module.txt                  manifest (below)
  LICENSE                     MIT
  include/mealkit/render/     public headers only
  src/                        implementation and private headers
  shaders/
  editor/                     editor panels of this module (built only in Editor builds)
  tests/                      TEST_CASE(render, ...), built into mealkit_tests and by the module's own CI
  third_party/                libraries only this module uses (Jolt in module-physics), same rules as the engine's
  .github/workflows/          one line calling the shared workflow ("Module CI")
```

- **Include paths don't change for games**: `#include "mealkit/render/Renderer.h"` still works. Private headers are in `src/` and can't be included from outside the module.
- A module's `editor/` folder holds the panels about it ([Look Dev] with `render`, [Sound] with `audio`). The `editor` module gathers them.
- Library code that doesn't need MealKit headers stays a separate library inside its module repo, checked by `check_libs.ps1` (`mkui` inside module-ui; `mkaudio` stays the engine-free runtime in MkAudio, with the glue module beside it).

### Manifest `module.txt`

The same file as a source module's (`docs/MODULES.md`), with `type engine` (since 2026-11-09; before, `engine-module.txt`):

```
@format module 1
type engine
name render
version 0.1.1
engine 0.9
kind view
layer framework
requires rhi 0.1 scene 0.1
tags render
summary Renderer (forward pipeline, lights, shadows, post, particles)
```

| Key | Meaning |
|---|---|
| `name` | CMake target `mealkit_<name>`, namespace `mealkit::<name>`, memory tag, folder under `modules/` once vendored |
| `version` | The module's semver, matching the repo tag `v<version>` |
| `type` | `engine` (a `module.txt` without it is a source module) |
| `engine` | The engine (kernel) MINOR it builds against, as for source modules |
| `layer` | `platform` (OS and API backends), `kernel`, `core` (simulation), `framework` (window, drawing, sound, input, UI), `app` (game, editor, debug tools). A module includes only its own layer and the layers below (`check_modules.ps1`) |
| `kind` | `sim` (in the simulation version hash, follows the determinism rules, allowed from `sim/`), `view` (not allowed from `sim/`), `tool` (editor and dev builds only, never in shipping) |
| `requires` | Modules this one may include and link, as `name minVersion`. Must form a graph without cycles. Replaces the folder order in `check_layers.ps1` |
| `provides` | Optional. For backend modules that implement another module's interface: `rhi-dx12` and `rhi-vulkan` both `provides rhi-backend`; a project picks one |
| `public` | Public headers, relative to `include/mealkit/<name>/`. Replaces that module's lines in `engine/public_headers.txt` |

`mealkit_add_engine_module()` in `cmake/MealKit.cmake` reads the manifest, creates the static library, and sets includes and links from `requires`. Modules don't write their own dependency lines, so the manifest and the build can't disagree. The format line and reading go into mkcontract, since the Manager and module CI read manifests too.

### Module selection and versions

Engine modules are chosen like source modules (decided 2026-11-09): the folders in the game's `modules/` are the modules it uses, one tool adds, updates and removes both kinds, and the editor's [Modules] window and the Manager list both.

```
powershell -ExecutionPolicy Bypass -File <engine>\tools\module.ps1 add -Preset standard -Project <folder>   # every engine module of a preset (new projects get this)
powershell -ExecutionPolicy Bypass -File <engine>\tools\module.ps1 add camera@0.1.2 -Project <folder>        # one module at a version (tag v0.1.2 of its repo)
powershell -ExecutionPolicy Bypass -File <engine>\tools\module.ps1 update render -Project <folder>           # to the engine's copy; -To <version> for another
powershell -ExecutionPolicy Bypass -File <engine>\tools\module.ps1 remove audio -Project <folder>
powershell -ExecutionPolicy Bypass -File <engine>\tools\module.ps1 list -Project <folder>
```

- Layers in use: `platform` = the kernel's `platform`; `kernel` = `core`, `jobs`; `core` = physics, sim, data, scene, asset, session, net; `framework` = rhi, render, input, audio, ui, loc, settings, feedback, camera, live (the dev connection, used by audio and game), online and its backends (a backend takes its interface's layer); `app` = game, editor, debug.
- `add <name>` takes the engine's copy (its `modules/<name>`, the locked version); `add <name>@<version>` takes tag `v<version>` of the module's repo from `modules.lock` (the checkout beside the engine when there is one). Files and where they came from go in `module.files`, so `update` replaces only files the game didn't change and writes `<file>.new` for the others, as for source modules. Copied: every file but `.git/`, `.github/`, `backends/` (separate modules), `CLAUDE.md`, `README*`.
- `remove` refuses while another module's `requires` lists it or game code includes `mealkit/<name>/` outside `#if MEALKIT_HAS_<NAME>` (`-Force` anyway).
- A game project builds with exactly the engine modules in its `modules/`; with none there it has only the kernel. The engine's own samples (`samples/<name>`) and the editor's `mealkit.dll` use every engine module, so engine changes reach the samples at once. Game code that includes a module the project lacks (outside `#if MEALKIT_HAS_<NAME>`) stops the configure step with the module names and the lines.
- Modules that require a missing module go with it (no `net` also means no `online`); only `game` stops configuring instead, naming the root module and the `module.ps1 add` line. `editor` is never part of a game. Kernel modules can't be left out.
- Code that works without a module has it as `optional` and keeps its uses inside `#if MEALKIT_HAS_<NAME>`; every module gets `MEALKIT_HAS_<NAME>` 0 or 1.
- Each module's `requires name minVersion` is checked against the versions in `modules/`. A source module's `requires` may name engine modules (a minimap needs `render`).
- The build compiles each module from the project's folder, with the engine's file list for that module (new `.cpp` files at the top of `src/mealkit/<name>/` are added). One set of engine libraries is built per distinct selection (`mealkit_core_<key>`, `mealkit_<key>`); the engine's full set is the plain `mealkit_core`/`mealkit`.
- The simulation version covers the kernel and the sim modules this build uses, from the folders it builds them from.
- The editor's `mealkit.dll` has every module at the engine's version; a project's own copies apply to its shipping and headless builds.
- Engine updates don't change a game's copies: the game moves each with `module.ps1 update`.
- Presets (`standard`; `solo` = without `net` and `online`) are only names for `add -Preset` and for the engine's own checks (`build.bat Presets`: `sandbox_solo`, `sandbox_noaudio`). The 0.9.1 project lines `enginePreset`/`engineModules`/`engineWithout`/`engineModuleVersion` are gone: `tools\upgrade_project.ps1` turns them into copies (`enginePreset standard` alone is just dropped).

### Module list (first cut = current folders)

The first cut follows the current folders, so it's mostly moving files. Finer splits (terrain out of `render`, ragdoll out of `sim`) wait until a game needs them.

| Module | Kind | Requires | Today |
|---|---|---|---|
| `physics` | sim | — | `physics/` Jolt wrapper (Jolt linked privately, as now) |
| `sim` | sim | physics | `sim/` MatchFlow, Movement, Ragdoll, NavGrid, Commands, Table |
| `data` | sim | — | `data/` table reading |
| `scene` | sim | data | `scene/` |
| `asset` | sim | data scene | `asset/` CPU data only: models, `MeshData`/`MaterialData`, terrain, scatter, cooking; GPU uploads through `asset::ModelGpu` (texture cache, sky HDRI and model drawing are in `render` since 2026-11-09) |
| `game-sim` | sim | sim data scene asset | `game/` Simulation, GameData, Loadout, InputRecord, Headless, the session interface (below) |
| `net` | sim | game-sim | `net/` lockstep, sockets, LAN rooms, host migration, mid-match joining |
| `live` | tool | — | `live/` dev connection server |
| `window` | view | — | `platform/Window`, `Input`, `Gamepad`, `FileDialog`, `ImGuiPlatform` |
| `rhi` | view | window | `rhi/` interface |
| `rhi-dx12`, `rhi-vulkan` | view | rhi | `rhi/dx12/`, `rhi/vulkan/` (`provides rhi-backend`) |
| `render` | view | rhi asset | `render/`, plus `ModelDraw` (`ModelUploader`, `addModel`/`addTerrain`/`addScatter`/`postParams`/`probes`), `TextureCache`, `Environment` |
| `loc`, `settings` | view | data | same folders |
| `input` | view | window data | `input/` |
| `audio` | view | data | `audio/` glue + mkaudio |
| `ui` | view | render loc input | `ui/` glue + `third_party/mkui` (MkUI repo) |
| `feedback`, `camera` | view | render audio | same folders |
| `online` | view | net | `online/` interface; `online-steam`, `online-eos` as `provides online-backend` |
| `debug` | tool | render | debug UI, ImGui |
| `game` | view | the `solo` preset | `game/` GameMain, SimulationGame, MatchService, Frontend, Cook |
| `editor` | tool | game + every module's `editor/` | `editor/` |

Every module also requires `core`, which isn't written.

### Multiplayer as a module

`game::MatchService` owns a `net::LockstepSession` today, and offline play runs through it. To make `net` optional:

- `game-sim` gets the **session interface** (`game::Session`: local players, confirmed inputs and commands for a tick, input delay, hashes for checks) and a `LocalSession` that confirms local input at once. Offline play, local multiplayer on one PC, replays and scenario tests use it.
- `net` provides `LockstepSession` implementing the same interface, plus everything only peers need (sockets, LAN rooms, relays through `online`, host migration, mid-match joining, desync dumps). One virtual call per tick, not per object.
- `MatchService` keeps `host`/`join`/`hostOnline`/`joinOnline` in every build; without `net` they fail with a message and `multiplayerAvailable()` is false, so one screen flow serves both presets (changed 2026-11-08 in step 7: a build error would force `#if` into every game's menu code). `playOffline` always works.
- The determinism rules, SimCheck and the simulation version stay for every game, with or without `net`.

### Presets

| Preset | Modules | Who uses it |
|---|---|---|
| `solo` | every module but `net`, `online`, `online-*` (and `editor`) | `module.ps1 add -Preset solo`; `build.bat Presets` builds `sandbox_solo` |
| `standard` | every module but `editor` | new projects (`tools\new_project.ps1` copies them into `modules/`) |

- The shipping exe links only the project's modules. The editor links all of them.
- Changing a project's modules rebuilds the game.

### Services and `GameContext`

- The kernel has a **service registry**: a module registers its long-lived objects at init (`kernel.provide<render::Renderer>(renderer)`), and others get them with `kernel.get<render::Renderer>()`. Init and shutdown run in dependency order (ties broken by name), printed with `--modules`, so the order is visible and the same on every PC.
- `GameContext` stays in the `game` module with its reference members (`ctx.renderer`, `ctx.audio`, ...), because `game` requires all of those. `ctx.session` becomes `game::Session&`. Games written today don't change beyond that.
- Modules outside the `solo` preset (`net`, `online`, a future `terrain`) are reached with `ctx.get<T>()`. Including the header already requires the module, so `get` can't return null.
- The kernel doesn't run a frame loop or call modules each frame. `game::GameMain` and the editor call subsystems in their written order, as now (`docs/CODING_MODEL.md` "Execution order and time").

### Determinism and the simulation version

- The simulation version hash covers the kernel and every `kind sim` module the project enables (their vendored sources), instead of the folder list in `cmake/BuildInfo.cmake`. Peers with different sim modules are refused, as now.
- SimCheck allows `sim/` (game and source modules) to include only the kernel and `kind sim` modules' public headers.
- A replacement physics module (`provides physics`, same public API) is a different simulation version and needs the full determinism group, including cross-platform CI, before any sample uses it.

### Engine modules and source modules

The docs say **engine module** (`type engine`: built into the engine libraries, in the editor's `mealkit.dll` too, one repo each) and **source module** (gameplay code built into the game, `docs/MODULES.md`). Both live in the game's `modules/<name>/` with a `module.txt`, and `tools\module.ps1` handles both the same way.

## Repositories

### What lives where

| Repo | Contents |
|---|---|
| `MealKit` | kernel (`engine/core/`), `cmake/` (`mealkit_add_game`, `mealkit_add_engine_module`), `tools/` (build, check, project and module tools), `templates/`, `samples/`, `modules.lock`, vendored modules in `modules/`, the integration CI, and the shared module workflow |
| `module-<name>` | one engine module each, from the table above (`module-render`, `module-physics`, `module-net`, ...) |
| `module-rhi` | `rhi` with its backends `rhi-dx12` and `rhi-vulkan` (one folder each under `backends/`, each still its own module and manifest), so a graphics change on both backends stays one commit |
| `module-online` | `online` with `online-steam` and `online-eos` the same way (SDKs stay outside the repo, stub approach) |
| `MkAudio` | the existing engine-free `mkaudio` runtime and tool, plus the `audio` glue module in `module/` |
| `module-ui` | `mkui`, the `ui` glue module, and later the Penpot plugin (pulls ROADMAP "UI 2" forward) |
| `module-editor` | the `editor` module (the general-purpose editor app) |

Decided 2026-11-11: the view-side libraries become engine-free middleware products MkRender, MkUI, MkAudio and later MkAnim, with glue modules, like MkAudio. See `docs/MIDDLEWARE.md`.

Repos are under `github.com/MealKit-Engine/`, MIT, KingProtea99 as author. The local checkouts sit beside the others in `D:\MealKit-Engine\`.

### Lock file and vendored copies

```
# MealKit/modules.lock
@format module-lock 1
module render  https://github.com/MealKit-Engine/module-render  v0.1.0  3f2a…
module physics https://github.com/MealKit-Engine/module-physics v0.1.0  9c1d…
```

- **An engine version is a tested set**: the root `VERSION` plus `modules.lock`. Games pin an engine version, not module versions one by one, so the combinations that exist are the ones CI tested.
- The engine repo **vendors** each locked module into `modules/<name>/` (source copies of `module.txt`, `include/`, `src/`, `shaders/`, `editor/`, `tests/`, `third_party/`, `LICENSE`), the way `third_party/mkaudio` works today. Cloning `MealKit` alone is still enough to build, offline, and the simulation version hashes what's in the folder.
- `tools\module.ps1 engine` keeps both in step (`tools\modules.ps1` until 2026-11-09, still forwarding): `sync` (copy the locked tags, check commits), `bump <name> [<tag>]` (update the lock and the copy in one engine commit), `status` (vendored copy vs lock vs local checkout). It generalizes `update_mkaudio.ps1`.
- **Local mode**: `tools\module.ps1 engine local on [name]` turns `modules/<name>` into a junction to the sibling checkout `..\module-<name>` (changed 2026-11-08 in step 9 from a CMake switch: the build keeps one set of paths), for changes that span repos; `local off` restores the vendored copy. When the change is done, each module repo gets a commit and tag, and `bump` brings them into the engine in one commit.

### Module CI (per module repo)

Each module repo has one workflow line calling `MealKit-Engine/MealKit/.github/workflows/engine-module.yml@v1`. It checks out `MealKit` at the newest tag matching `kernel`, replaces that module's vendored copy with the repo under test, builds the `kernel` preset plus that module and its `requires`, runs its `tests/`, and for `kind sim` modules runs the headless determinism group on Windows and Linux. The engine repo's own CI builds every preset, samples, goldens and the full determinism group against the lock.

## Checks

| Today | After |
|---|---|
| `check_layers.ps1` folder order | `check_modules.ps1`: manifests parse, the graph has no cycles, each module includes only itself, `core` and its `requires`, `kind sim` modules include no `view` module, vendored copies match the lock |
| core source list in `check_layers.ps1` | gone: the `kernel` and `headless` presets are real targets that fail to build if a view header leaks in |
| `engine/public_headers.txt` | per-module `public` lines; the script gathers them. CONTRACTS paths stay `mealkit/<module>/<header>` |
| `check_platform.ps1` | same rule: OS APIs in `core/src/platform/<os>/` and `window`, graphics APIs in `rhi-<api>` |
| `check_libs.ps1` (mkaudio, mkui) | unchanged |
| CI | the engine builds `kernel`, `headless`, `solo` and `standard`, plus Vulkan; exe size reported per preset. Each module repo runs the shared workflow |

## Build order

All steps are done now, before publication. Each is one ROADMAP item, done when the Editor and Release builds, render goldens on both backends, and the determinism group pass. The cut happens inside the `MealKit` repo first (one build, atomic commits, easy to bisect), and each module folder is then extracted into its repo **with its history** (`git filter-repo`, as on 2026-11-03), so the public repos start with real history.

1. **Kernel memory**: `core/Memory.h` (heap tags, `Arena`, `FixedPool`, frame and match arenas), stats in `FrameStats` and a [Debug] > [Memory] page, Jolt/miniaudio hooks. `Pool` storage from the match arena.
2. **Manifests on the current folders** (`engine-module.txt` in each engine folder) and `check_modules.ps1` replacing `check_layers.ps1`. Still one library. Only the script reads manifests yet; reading in mkcontract comes with the first C++ or Manager reader (step 7), and `public` lines with the public header move (step 4).
3. **Kernel target**: `engine/core/` becomes `mealkit_core` with its own include folder; `sim/` containers move into it.
4. **Module targets, leaves first**, in `modules/<name>/` with the repo layout above: `camera`, `feedback`, `settings`, `loc`, `audio`, `ui`, then `render`, `rhi` and its backends, then `asset`, `scene`, `data`, `physics`, `sim`, `game-sim`, `net`, `online`, `game`, `editor`. Include paths stay the same.
5. **Service registry and init order** in the kernel; `GameContext::get<T>()`.
6. **Session interface**: `game::Session`, `LocalSession`, `net` optional; the `solo` preset builds and plays sandbox offline.
7. **Project module lists and presets**, shipping links only those, `upgrade_project.ps1` migration, `templates/project` change (bumps `VERSION`), `kApiVersion` bump. Exe size measured per preset.
8. **Editor panels into modules' `editor/` folders**; `online-steam`/`online-eos` as backend modules.
9. **Repos**: extract each `modules/<name>/` with history into `D:\MealKit-Engine\module-<name>` (local git, KingProtea99, tag `v0.1.0`), `modules.lock`, `tools\modules.ps1`, local mode, the shared module workflow. Creating the GitHub repos and pushing is done by the user.
10. **Workspace and docs**: workspace `CLAUDE.md` repo list, `mk-build.ps1` and `.clangd` for module repos, the CLAUDE.md layer rules, `docs/CODING_MODEL.md`, `docs/CONTRACTS.md`, the docs repo's engine developer guide (English and Korean), the publication order in ROADMAP.

## Cost and risk

- Steps 3–4 move most engine files. Every include of a private header from another folder becomes a build error that needs a public header or a `requires` line; the layer check already removed the backward ones, but the count isn't measured yet.
- The editor build, golden tests and determinism replays must stay green through the moves. Moving files rebuilds everything, so each step is a full rebuild through `mk-build.ps1`.
- After step 9 a change that spans the kernel and a module is several commits in several repos plus a lock bump. Backends share their interface's repo, so the RHI rule "DX12 and Vulkan fixed in the same piece of work" stays one commit in `module-rhi`; a graphics feature usually also touches `module-render`, so it's two repos plus a lock bump.
- The 2026-11-03 contract review needs a pass over `docs/CONTRACTS.md` and the cookbook. Doing this before publication keeps it a 0.x change.
- Rough size: steps 1–2 a few days, 3–8 two to three weeks, 9–10 about a week.

## Decisions (2026-11-08, with the user)

1. Goal: a small kernel that rarely changes and smaller exes per game, leaving room for replacing implementations and for others' modules.
2. Static modules chosen at build time; no runtime plugins.
3. `net` is an optional module (in the default `standard` preset). The absolute rule becomes "multiplayer by default"; the determinism rules stay for every game.
4. One repo per module from the first public release, with the lock file and vendored copies in `MealKit`. Backend modules live in their interface's repo (`module-rhi`, `module-online`).
5. Everything is built now, before publication.

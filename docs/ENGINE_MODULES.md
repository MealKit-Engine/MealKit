# Engine modules

How the engine is split: a small kernel, and every subsystem an engine module **in its own repo**, plus the view-side middleware libraries (MkUI, MkAudio, MkRender) behind glue modules. Decided in `docs/DECISIONS.md` 2026-11-08 "Engine module split" and 2026-11-11 "View-side middleware"; built 2026-11-08~09 (steps 1~13 in `docs/CHANGELOG.md` "Engine module split"). Source modules (gameplay code copied into a game) are in `docs/MODULES.md`.
This reversed `docs/DECISIONS.md` 2026-10-19 ("no toggleable build modules, a folder layer order instead") and 2026-10-26 "When splitting out subsystems" ②, and changed the absolute rule "always multiplayer-capable" to "multiplayer by default".

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
- **The frame arena** replaces per-frame `std::vector` growth found by the frame allocation check, only where measurement shows allocations.
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

### Module list

The split follows the old engine folders. Finer splits (terrain out of `render`, ragdoll out of `sim`) wait until a game needs them.

| Module | Kind | Requires | Contents |
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
| `MkUI` | the engine-free `mkui` runtime, MkUI Designer and the Penpot plugin (pinned in `third_party/mkui`) |
| `module-ui` | the `ui` glue module |
| `module-editor` | the `editor` module (the general-purpose editor app) |

The view-side libraries are engine-free middleware products with glue modules ("Middleware" below).

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

| Check | What it does |
|---|---|
| `check_modules.ps1` | manifests parse, the graph has no cycles, each module includes only itself, `core` and its `requires`, `kind sim` modules include no `view` module, vendored copies match the lock |
| presets as targets | the `kernel` and `headless` builds fail if a view header leaks in |
| public headers | per-module `public` lines, gathered by the script; CONTRACTS paths stay `mealkit/<module>/<header>` |
| `check_platform.ps1` | OS APIs in `core/src/platform/<os>/` and `window`, graphics APIs in `rhi-<api>` |
| `check_libs.ps1` | engine-free libraries (mkaudio, mkui) include no MealKit header |
| CI | the engine builds `kernel`, `headless`, `solo` and `standard`, plus Vulkan; exe size reported per preset. Each module repo runs the shared workflow |

## Decisions (2026-11-08, with the user)

1. Goal: a small kernel that rarely changes and smaller exes per game, leaving room for replacing implementations and for others' modules.
2. Static modules chosen at build time; no runtime plugins.
3. `net` is an optional module (in the default `standard` preset). The absolute rule becomes "multiplayer by default"; the determinism rules stay for every game.
4. One repo per module from the first public release, with the lock file and vendored copies in `MealKit`. Backend modules live in their interface's repo (`module-rhi`, `module-online`).
5. Everything is built before publication.

## Middleware (view-side libraries)

Some engine modules are **engine-free middleware libraries** that other engines can use too, the way `mkaudio` (MkAudio) is. Status (2026-11-12): MkUI has its own repo with the draw backend, view models without reflection and MkUI Designer; MkRender has its own repo with `mkrhi` (graphics layer) and `mkrender` (the renderer and its shaders), glued by `modules/rhi` and `modules/render`; MkUI's XAML-like layer, MkRender's engine-free goldens and MkAnim are not started (work items in `docs/ROADMAP.md` "UI 2" and "Tool suite").

### The idea

| Product | Replaces | Library | Engine glue module |
|---|---|---|---|
| MkRender | (MealKit's own renderer and visual tools) | `mkrender` + `mkrhi` | `render`, `rhi` |
| MkUI | NoesisGUI | `mkui` | `ui` |
| MkAudio | FMOD | `mkaudio` (exists) | `audio` (exists) |
| MkAnim | Spine | `mkanim` | `anim2d` |
| MealKit (engine) | — | kernel + simulation stack | lifecycle and module management |

### Names

Each product is a plain field name with the common prefix **`Mk`**: MkRender, MkUI, MkAudio, MkAnim. The prefix matches the code names already in use (`mkaudio`, `mkui`, `mkcontract`), so product, repo (`MkRender`, `MkUI`, `MkAudio`, `MkAnim`), library target and namespace (`mkrender::`) are one name. Tools follow the same rule (MkAudio Tool, MkUI Designer (exe `MkUI`, MkUI `tool/`), MkAnim Editor). The engine keeps the name MealKit and binds the products through its glue modules; the products don't carry "MealKit" in their names, so other engines can use them without looking like engine add-ons. MealKit-Audio was renamed MkAudio on 2026-11-11 (repo, tool exe `MkAudio`, tool title); the tool keeps its old settings folder.

### The pattern every middleware follows

Taken from MkAudio, which already works this way:

1. **The library includes no MealKit header.** C++20, its own namespace (`mkui::`, `mkrender::`), its own small file formats with an `@format` line, tests that run without the engine (`check_libs.ps1`).
2. **It gets the outside world through narrow interfaces**: an allocator hook, a log callback, a file reader, and (for UI and Anim2D) a draw backend that receives vertex/index lists and texture ids. The engine's glue module implements them with `mem::heap(tag)`, `Log`, packs and the renderer.
3. **The glue module** (`type engine`, as above) connects the library to `GameContext`, the editor panels and cooking. Games only see the glue module, so their code doesn't change.
4. **Its own tool** where the product needs one (the MkAudio tool today; a UI preview/designer, a 2D rig editor), using mkcontract for the user folder and the live connection.
5. **View only.** No middleware is `kind sim`. Nothing in it enters the simulation version, so these libraries are free of the determinism rules. That's also why physics, `sim`, `net` and `game-sim` are **not** middleware candidates (below).

### Is each one possible, and is it worth it?

#### UI — MkUI (NoesisGUI alternative) — possible, recommended first

- **Where it stands**: `mkui` is engine-free and lives in its own repo MkUI since 2026-11-11 (pinned in `third_party/mkui`): Flex layout, multi-style text, SDF shapes and text, widgets, focus, themes, components, lists, world-space UI, a draw backend interface (vertex/index lists + texture ids), view models without reflection (`ValueModel`; the glue module keeps `MEALKIT_VIEWMODEL`), and MkUI Designer with the Penpot plugin (UI 2a).
- **What "XAML-style" adds**: markup that declares the tree; **data binding** (`{Binding Health}`, one-way/two-way, converters) to view models; **styles and templates** (a control's look is replaceable data); **triggers and state animations** (hover, pressed, focused, value changes); resource dictionaries (shared colors, brushes, styles). All of this fits on top of the current `.ui` runtime.
- **What not to promise**: XAML/WPF compatibility. Noesis is a full WPF reimplementation (hundreds of classes, years of work). MkUI should be *XAML-like* — the same ideas, its own text format — and say so. Loading real `.xaml` files is a later, optional importer for a subset.
- **Worth it**: yes. Game UI is something every engine needs, and MkUI Designer is the product's designer.

#### Rendering — MkRender — possible, worth it only as a clean boundary

- **Where it stands** (2026-11-12): the repo MkRender holds `mkrhi` (DX12/Vulkan backends with a native window handle, log callback, capture hook) and `mkrender` (the forward renderer, `DrawList` culling with a host parallel-for, `shaders/` and their DXC step, a shader reload hook, captures as plain pixels), pinned in `third_party/mkrender`. Host types come in like Dear ImGui's `imconfig.h`: MealKit's `render/MkRenderConfig.h` makes `core/Math.h` and `asset/MeshData.h` types mkrender's, so engine and game code are unchanged. `modules/render` keeps the engine-side parts: `render::Renderer` (job system, GPU capture, hot reload, PNG screenshots, Image conversion), `ModelDraw`, `TextureCache`, `Environment`, `Sprites`, `SceneLights`, Look Dev; `render/Fwd.h` replaces forward declarations of render types.
- **What was needed** (done): `mkrhi` with no `window` dependency (takes a native handle and size); `mkrender` taking its own plain types (mesh buffers, material parameters, lights, camera, sprites) instead of `asset::MeshData`/`scene` types, with the glue module converting; shaders compiled by the library's own build step.
- **Scope**: the forward pipeline, lights, shadows, post, particles, sprites, and the visual tools that sit on it (Look Dev, material preview, render goldens). Not the editor viewport, not cooking.
- **Honest assessment**: possible, but the largest of the four (most of the code and most cross-module types), and the market (bgfx, Filament, Diligent, The Forge) is crowded and free. The real benefit is internal: a renderer with plain-data inputs is easier to test (goldens without the engine) and to replace. Recommended as **"make the boundary, publish later"**: cut `mkrender`/`mkrhi` inside `module-render`/`module-rhi` when a render item touches those types anyway, and decide about a separate product after that.

#### 2D animation — MkAnim (Spine alternative) — possible, but defer

- **What Spine is**: a skeleton (bones, slots, attachments), meshes with weights, IK and path constraints, skins, events, clip blending — and above all **the editor** where artists rig and animate. The runtime is a few thousand lines; the editor is most of the product.
- **Where MealKit stands**: the sprite pass and the `sprite` object exist; flipbook animation isn't done yet (ROADMAP "2D on the 3D engine"). No 2D game needs skeletal animation yet.
- **Formats**: Spine's runtimes have a license that requires a Spine editor license, so MealKit can't build on them or ship Spine-file reading as a free replacement. DragonBones' format is MIT and has free exporters, so an `mkanim` runtime could read a DragonBones-compatible subset first and leave the rig editor for later.
- **Assessment**: technically realistic for the runtime (skeleton, weighted meshes, 2-bone IK, blending, events into the feedback table), but its value depends entirely on an editor. **Defer** until a 2D game asks for it; finish flipbooks first. When started: runtime + importer first, rig editor as its own tool much later.

#### Audio — MkAudio (FMOD alternative) — already planned

Already done in the pattern above (`mkaudio` + tool + `audio` glue in MkAudio). It is the template for the others.

### What stays in the game engine

The user's draft says the engine keeps "lifecycle and middleware management". That's right, **plus the parts that make MealKit what it is**, which can't be middleware because they're simulation (`kind sim`) and must share one determinism contract:

| Stays in the engine | Why it isn't middleware |
|---|---|
| **Kernel**: memory, math and `DetMath`, reflection, text formats, logging/profiling, platform basics, jobs | Everything else is built on it |
| **Lifecycle**: `GameMain`, the frame loop and its written order, fixed tick, interpolation, pause, `GameContext` | Owns the call order between modules (`docs/CODING_MODEL.md` "Execution order and time") |
| **Module management**: manifests, init/shutdown order, service registry, presets, `modules.lock` | The engine's job by definition |
| **Simulation stack**: `physics` (Jolt wrapper), `sim`, `game-sim`, `Session`, `net` (lockstep, rooms, host migration), replays, SimCheck, the simulation version | Determinism across peers is one contract over all of them; splitting it into products would split the guarantee |
| **World data**: `scene`, `asset`, `data` (tables, tuning), cooking and packs | The shared language every module and tool reads |
| **Platform glue**: `window`, `input`, `settings`, `loc`, `online` backends | Thin, game-specific, not worth a product |
| **Game-facing view modules**: `camera`, `feedback` | They combine render + audio + UI per game; they're where middleware meets gameplay |
| **Tools**: the editor, `live`, `debug`, QA reports, upgrade tools | They integrate everything; each middleware brings its own panel through its glue module's `editor/` |
| **Glue modules** for each middleware | Implement the middleware's interfaces with engine services |

In short: **MealKit stays "a deterministic multiplayer game engine"**, and the middleware are the view-side parts that are good on their own.

### Against the "external first" rule

CLAUDE.md's absolute rules (ROADMAP criterion ④, `docs/DECISIONS.md` 2026-10-26) say MealKit builds only the simulation, lockstep networking, renderer and editor, and makes its own tool only where no suitable **free** tool exists. Checked per product:

| Product | Free alternative? | Fits the rule? |
|---|---|---|
| Render | — (the renderer is one of the four things MealKit builds) | yes |
| Audio | FMOD/Wwise are commercial; no free data-driven event runtime with a designer tool | yes (already decided) |
| UI | Noesis is commercial. RmlUi (MIT) is free but HTML/CSS, not markup + binding + templates; and `mkui` already exists | yes, if the goal is XAML-like binding and templates, which RmlUi doesn't give |
| Anim2D | Spine is commercial; DragonBones (MIT) is free but its editor is no longer maintained | borderline: check DragonBones/other free runtimes first when the time comes |

The rule's wording gains "and the view-side middleware in `docs/ENGINE_MODULES.md`".

### Costs

- Each middleware needs its **own stable formats and API** and its own versioning, beyond what the engine needs. A breaking change becomes a library release plus a glue bump plus a lock bump.
- Each one needs the **interfaces in the pattern** (allocator, log, files, draw backend). For UI and Anim2D the draw backend is new work; for render, cutting `asset`/`scene` types is the big part.
- More repos to keep green for one maintainer. Mitigated by the same module CI and local-checkout mode as engine modules.
- Docs: each product gets its own README and a page in the docs repo; the engine manual links to them.

### Proposed order

1. **UI**: ~~own repo, draw backend, public API, Designer and Penpot plugin~~ (done 2026-11-11~12), then binding, styles/templates, triggers and resources.
2. **Render**: ~~own repo MkRender with `mkrhi` and `mkrender`~~ (done 2026-11-12), then goldens without the engine. Decide on publishing separately after.
3. **Anim2D**: after flipbooks, and only when a 2D game needs skeletal animation. Runtime + DragonBones-subset importer first.

### Decisions (2026-11-11, with the user)

1. Names: plain field names with the prefix `Mk` (MkRender, MkUI, MkAudio, MkAnim); MealKit-Audio is renamed MkAudio.
2. MkUI is XAML-*like*: markup, binding, styles/templates, triggers and resources in its own format. `.xaml` compatibility is not promised.
3. MkRender: the boundary first (`mkrhi`/`mkrender` with plain-data inputs inside their module repos); publishing it as a separate product is decided later. *Changed 2026-11-12: MkRender gets its own repo now, like MkUI (`docs/DECISIONS.md` 2026-11-12).*
4. MkAnim is deferred until a 2D game needs skeletal animation (flipbooks first).

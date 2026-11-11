# Middleware split

**Status: decided 2026-11-11 with the user (`docs/DECISIONS.md` 2026-11-11 "View-side middleware"). MkUI has its own repo (v0.2.0, 2026-11-11: draw backend and view models without reflection done); its XAML-like layer and the others are not started.** Extends `docs/ENGINE_MODULES.md`: some engine modules become **engine-free middleware libraries** that other engines can use too, the way `mkaudio` (MkAudio) already is. Work items are in the ROADMAP ("Tool suite"); this document follows the same life as `ENGINE_MODULES.md`.

## The idea

| Product | Replaces | Library | Engine glue module |
|---|---|---|---|
| MkRender | (MealKit's own renderer and visual tools) | `mkrender` + `mkrhi` | `render`, `rhi` |
| MkUI | NoesisGUI | `mkui` | `ui` |
| MkAudio | FMOD | `mkaudio` (exists) | `audio` (exists) |
| MkAnim | Spine | `mkanim` | `anim2d` |
| MealKit (engine) | — | kernel + simulation stack | lifecycle and module management |

## Names

Each product is a plain field name with the common prefix **`Mk`**: MkRender, MkUI, MkAudio, MkAnim. The prefix matches the code names already in use (`mkaudio`, `mkui`, `mkcontract`), so product, repo (`MkRender`, `MkUI`, `MkAudio`, `MkAnim`), library target and namespace (`mkrender::`) are one name. Tools follow the same rule (MkAudio Tool, MkUI Designer, MkAnim Editor). The engine keeps the name MealKit and binds the products through its glue modules; the products don't carry "MealKit" in their names, so other engines can use them without looking like engine add-ons. MealKit-Audio was renamed MkAudio on 2026-11-11 (repo, tool exe `MkAudio`, tool title); the tool keeps its old settings folder.

## The pattern every middleware follows

Taken from MkAudio, which already works this way:

1. **The library includes no MealKit header.** C++20, its own namespace (`mkui::`, `mkrender::`), its own small file formats with an `@format` line, tests that run without the engine (`check_libs.ps1`).
2. **It gets the outside world through narrow interfaces**: an allocator hook, a log callback, a file reader, and (for UI and Anim2D) a draw backend that receives vertex/index lists and texture ids. The engine's glue module implements them with `mem::heap(tag)`, `Log`, packs and the renderer.
3. **The glue module** (`type engine`, as in `ENGINE_MODULES.md`) connects the library to `GameContext`, the editor panels and cooking. Games only see the glue module, so their code doesn't change.
4. **Its own tool** where the product needs one (the MkAudio tool today; a UI preview/designer, a 2D rig editor), using mkcontract for the user folder and the live connection.
5. **View only.** No middleware is `kind sim`. Nothing in it enters the simulation version, so these libraries are free of the determinism rules. That's also why physics, `sim`, `net` and `game-sim` are **not** middleware candidates (below).

## Is each one possible, and is it worth it?

### UI — MkUI (NoesisGUI alternative) — possible, recommended first

- **Where it stands**: `mkui` is engine-free and lives in its own repo MkUI since 2026-11-11 (pinned in `third_party/mkui`) (Flex layout, multi-style text, SDF shapes and text, widgets, focus, themes, components, lists, world-space UI, view models via `MEALKIT_VIEWMODEL`). Half the boundary exists.
- **What "XAML-style" adds**: markup that declares the tree; **data binding** (`{Binding Health}`, one-way/two-way, converters) to view models; **styles and templates** (a control's look is replaceable data); **triggers and state animations** (hover, pressed, focused, value changes); resource dictionaries (shared colors, brushes, styles). All of this fits on top of the current `.ui` runtime.
- **What not to promise**: XAML/WPF compatibility. Noesis is a full WPF reimplementation (hundreds of classes, years of work). MkUI should be *XAML-like* — the same ideas, its own text format — and say so. Loading real `.xaml` files is a later, optional importer for a subset.
- **What's missing for a standalone library**: the draw backend interface (today it draws through the engine's 2D pass), a font/image loader hook, and a public C++ API to create views and push view-model values without `MEALKIT_VIEWMODEL` (the glue module keeps the macro).
- **Worth it**: yes. Small step from today, game UI is something every engine needs, and the existing Penpot plugin plan (ROADMAP "UI 2") becomes the product's designer.

### Rendering — MkRender — possible, worth it only as a clean boundary

- **Where it stands**: `rhi` is already an interface with DX12 and Vulkan backends, and needs only a native window handle. `render` requires `asset` (CPU mesh/material data and `ModelGpu`) and draws from a `DrawList`.
- **What's needed**: `mkrhi` with no `window` dependency (takes a native handle and size); `mkrender` taking its own plain types (mesh buffers, material parameters, lights, camera, sprites) instead of `asset::MeshData`/`scene` types, with the glue module converting; shaders compiled by the library's own build step.
- **Scope**: the forward pipeline, lights, shadows, post, particles, sprites, and the visual tools that sit on it (Look Dev, material preview, render goldens). Not the editor viewport, not cooking.
- **Honest assessment**: possible, but the largest of the four (most of the code and most cross-module types), and the market (bgfx, Filament, Diligent, The Forge) is crowded and free. The real benefit is internal: a renderer with plain-data inputs is easier to test (goldens without the engine) and to replace. Recommended as **"make the boundary, publish later"**: cut `mkrender`/`mkrhi` inside `module-render`/`module-rhi` when a render item touches those types anyway, and decide about a separate product after that.

### 2D animation — MkAnim (Spine alternative) — possible, but defer

- **What Spine is**: a skeleton (bones, slots, attachments), meshes with weights, IK and path constraints, skins, events, clip blending — and above all **the editor** where artists rig and animate. The runtime is a few thousand lines; the editor is most of the product.
- **Where MealKit stands**: the sprite pass and the `sprite` object exist; flipbook animation isn't done yet (ROADMAP "2D on the 3D engine"). No 2D game needs skeletal animation yet.
- **Formats**: Spine's runtimes have a license that requires a Spine editor license, so MealKit can't build on them or ship Spine-file reading as a free replacement. DragonBones' format is MIT and has free exporters, so an `mkanim` runtime could read a DragonBones-compatible subset first and leave the rig editor for later.
- **Assessment**: technically realistic for the runtime (skeleton, weighted meshes, 2-bone IK, blending, events into the feedback table), but its value depends entirely on an editor. **Defer** until a 2D game asks for it; finish flipbooks first. When started: runtime + importer first, rig editor as its own tool much later.

### Audio — MkAudio (FMOD alternative) — already planned

Already done in the pattern above (`mkaudio` + tool + `audio` glue in MkAudio). It is the template for the others.

## What stays in the game engine

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

## Against the "external first" rule

CLAUDE.md's absolute rules (ROADMAP criterion ④, `docs/DECISIONS.md` 2026-10-26) say MealKit builds only the simulation, lockstep networking, renderer and editor, and makes its own tool only where no suitable **free** tool exists. Checked per product:

| Product | Free alternative? | Fits the rule? |
|---|---|---|
| Render | — (the renderer is one of the four things MealKit builds) | yes |
| Audio | FMOD/Wwise are commercial; no free data-driven event runtime with a designer tool | yes (already decided) |
| UI | Noesis is commercial. RmlUi (MIT) is free but HTML/CSS, not markup + binding + templates; and `mkui` already exists | yes, if the goal is XAML-like binding and templates, which RmlUi doesn't give |
| Anim2D | Spine is commercial; DragonBones (MIT) is free but its editor is no longer maintained | borderline: check DragonBones/other free runtimes first when the time comes |

If decided, the rule's wording gains "and the view-side middleware in `docs/MIDDLEWARE.md`".

## Costs

- Each middleware needs its **own stable formats and API** and its own versioning, beyond what the engine needs. A breaking change becomes a library release plus a glue bump plus a lock bump.
- Each one needs the **interfaces in the pattern** (allocator, log, files, draw backend). For UI and Anim2D the draw backend is new work; for render, cutting `asset`/`scene` types is the big part.
- More repos to keep green for one maintainer. Mitigated by the same module CI and local-checkout mode as engine modules.
- Docs: each product gets its own README and a page in the docs repo; the engine manual links to them.

## Proposed order

1. **UI**: draw backend interface and public API for `mkui`, then binding, styles/templates, triggers and resources. Move `mkui` to its own repo `MkUI` (like MkAudio), `module-ui` becomes glue only. Penpot plugin and a preview tool follow.
2. **Render boundary**: `mkrhi`/`mkrender` with plain-data inputs inside their module repos, when render work touches those types. Decide on publishing separately after.
3. **Anim2D**: after flipbooks, and only when a 2D game needs skeletal animation. Runtime + DragonBones-subset importer first.

## Decisions (2026-11-11, with the user)

1. Names: plain field names with the prefix `Mk` (MkRender, MkUI, MkAudio, MkAnim); MealKit-Audio is renamed MkAudio.
2. MkUI is XAML-*like*: markup, binding, styles/templates, triggers and resources in its own format. `.xaml` compatibility is not promised.
3. MkRender: the boundary first (`mkrhi`/`mkrender` with plain-data inputs inside their module repos); publishing it as a separate product is decided later.
4. MkAnim is deferred until a 2D game needs skeletal animation (flipbooks first).

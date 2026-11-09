# MealKit engine roadmap

Features to put in the engine and their order. Games aren't built in this repo. Features are chosen based on what games needed in other engines (Unity, Unreal, Godot, etc.),
and each feature is verified with the reference samples in `samples/` (stage 1 "Reference samples").
Finished items are shortened to one `[x]` line and implementation details move to `docs/CHANGELOG.md`. When the order changes, update this document. Reasons for decisions and features not included or deferred (the "Deferred" table) are in `docs/DECISIONS.md`.

## Decision criteria

Every feature is weighed in the order below to decide whether it goes in.

1. **Does it not hurt 3D + multiplayer?** Features touching the simulation must keep lockstep determinism (`docs/CODING_MODEL.md` "Determinism rules").
2. **Lightness and speed.** The target hardware is office PCs with Intel integrated graphics, **minimum spec Intel 11th gen (Xe integrated graphics) or newer**. The budgets below are what the default preset guarantees, not a limit on what the engine supports: features exceeding them are opt-in (off by default, no GPU cost when off, turned on per project, player-toggleable through quality presets unless the game locks them; `docs/DECISIONS.md` 2026-11-04).
3. **Multiple platforms, DX12 as reference.** OS APIs are called only behind the platform layer (`mealkit/platform/`), graphics APIs only behind the thin RHI (`mealkit/rhi/`).
   New rendering features are designed on the DX12 backend first, and **from stage 2 the Vulkan backend is built together within the same item**. Metal, WebGPU, and console backends (stage 5) follow. Features aren't lowered to fit other APIs; features a backend can't do are turned off in that platform's preset.
   The editor is Windows first, macOS/Linux in stage 5. The goal is crossplay between all platforms (platform table and conditions in stage 5).
4. **Use finished things instead of building them.** The engine repo handles only the deterministic simulation (+ physics glue), lockstep networking, renderer, and editor (placement, import, checks, debugging, tool shortcuts).
   - **External authoring tools** (Blender, Penpot, the Effekseer editor, VS Code, DaVinci, etc.) are used actively. They don't go into the game exe, so they cost nothing. The engine builds only import, checks, previews, and plugins for the tools.
   - **MealKit tools**: built only when no free tool fits a role's workflow (the first is audio). Separate repos outside the engine repo, MIT, one contract between tools ("Tool suite" below, `docs/DECISIONS.md` 2026-10-26).
   - **Runtime libraries** (linked into the exe) are also preferred over building our own. With conditions:
     freely if used only on the view side (`view/`), only determinism-verified ones if they touch the simulation (like Jolt). Source included, version pinned (`third_party/`), permissive license (MIT/BSD/zlib/public domain, etc.), within the shipping exe budget (5 MB).
     Libraries that run on every target platform are looked at first. One platform's native APIs (XAudio2, XInput, WIC, etc.) are used only inside that platform's implementation in the platform layer.

### Performance budget of the default preset (minimum spec Intel 11th gen Xe integrated graphics, 1280×720, 60 fps)

"Current" values were measured on a Core Ultra 5 125U (Xe-LPG), faster than the minimum spec. They must be re-measured on minimum spec hardware.
This table is for Windows. Minimum specs and budgets for other platforms are set in a table of the same shape when that platform is started (stage 5). The 5 MB executable is the same on all platforms.

| Item | Budget | Current |
|---|---|---|
| Whole frame (GPU) | 8 ms or less (half left as headroom at 60 fps) | about 2.2 ms (arena, shadows/post on: opaque 1.33, shadow 0.39, post 0.45, 2026-10-27 `--benchmark 20`) |
| Simulation tick (30 Hz) | 4 ms or less per tick | 0.9~2.5 ms (2,000 bodies) |
| Draw calls | 200 or fewer | 26 (816 instances, arena) |
| Network (4 players, host send) | 10 KB/s or less | about 5 KB/s |
| Game executable | 5 MB or less (excluding assets) | about 3.6 MB (Export build, including audio, Jolt, post, 2026-11-03) |

## Versions

The engine version is `VERSION` at the repo root. **v0.n = the engine having finished stage n** (stage 0 → v0.4 inherited the previous version number). **Everything before v1.0 is beta**: version numbers only mark progress through the stages and promise nothing. v1.0 is when the user declares the engine finished. During 0.x the `GameInstance` API (`kApiVersion`) and file formats may change, and existing projects are migrated with the upgrade tool.

| Version | Meaning |
|---|---|
| v0.4 | Stage 0: editor, hot reload, determinism checks, debugging tools. When the template changed while finishing stage 1 items, 0.4.x was bumped |
| v0.5 (now 0.5.0, 2026-10-19) | Stage 1: features to ship one game end to end (platform layer, data separation, sound, UI, input, menus, saves). The reference sample runs end to end from title to results screen. When the template changes while finishing stage 2 items, 0.5.x is bumped |
| v0.6 / v0.7 (now 0.7.0, 2026-11-01) | Graphics (RHI: DX12 + Vulkan) / stronger multiplayer. Stage 2 finished while the version stayed 0.5.x, so the version went 0.5.4 → 0.7.0 when stage 3 finished (no 0.6.0 release) |
| v0.8 (now 0.8.0, 2026-11-02) | Stage 4: gameplay framework and tools (game flow, movement modes, active ragdoll, animation states, virtual cameras, AI pathfinding, prefabs, terrain, Job System, spatial audio, pack files), two reference samples (`samples/sandbox`, `samples/coop`) and an empty project template |
| v0.9 (now 0.9.0, 2026-11-03) | v1.0 candidate: the contract freeze review is done and the cookbook docs are written; after it only fixes until v1.0 (`docs/DECISIONS.md` 2026-11-03). Decided exceptions, which only add to frozen formats: the game UI editor and designer independence D1~D9 (`docs/DECISIONS.md` 2026-11-04) |
| **v1.0** | **Windows** (editor and shipping, DX12 and Vulkan) is fully supported and **Linux x64 / macOS ARM64 run the headless simulation** (replays, scenario tests, headless peers; per-tick hashes match Windows in CI on every commit). All reference samples pass Export and the determinism checks on Windows, and **contracts are frozen**: the `GameInstance` API, file formats (`.scene`, `.tuning`, `.table`, `.ui`, `mealkit.project`, saves), `sim/` rules (SimCheck) |
| v1.x | Stage 5 tier 1 without breaking the frozen contracts: Linux/Steam Deck and macOS shipping and editors, cross-platform determinism between real devices, crossplay (EOS). Tier 2 (mobile, web, consoles) after |
| v1.x and later | Frozen contracts aren't broken. If one truly must break, the major version (v2.0) is bumped and the upgrade tool migrates |

## What exists now (v0.5, stage 1 done)

Added in stage 1: the platform layer (`win32/`, `posix/`), headless simulation and input recording/playback, CI (Windows MSVC/clang-cl, Linux x64/macOS ARM64 per-tick hash comparison), data separation (tuning, view tuning, definition data tables, project settings, time in seconds), sound (events, buses, voice management), events → feedback table, input actions/gamepads, confirmed commands, debug UI, string tables, game screen UI (`.ui` runtime, text input), player settings and the standard settings screen, saves, loadouts,
basic screen flow (title → menu → lobby → match → results → lobby, `game::MatchService`, LAN room discovery, chat), network condition simulation, player-facing network display, automatic input delay adjustment, predicting my character's appearance, accessibility basics, tick rate setting (30/60), version display and connection refusal reasons. Details in `docs/CHANGELOG.md` v0.4.1~v0.5.0.

Up to stage 0:

Windows only. DX12 forward renderer (instancing, vertex colors, DX12 called directly without an RHI), Jolt physics (deterministic, snapshots), input-only lockstep (UDP/Steam), glTF models + LOD,
`.scene` scenes (game-defined object kinds, parent/child hierarchy), general-purpose editor (project manager, game DLL hot reload, gizmos, undo, autosave,
tuning/state panels, solo/multiplayer play, shadow simulation, pause/rewind, shipping builds), field declarations (`MEALKIT_FIELD`), deterministic math,
the `sim/` check (SimCheck), Tracy profiling, project template and upgrade tool, verification script (`tools/check.ps1`).

**Networking approach** (`net::LockstepSession`):
- **Host-client (listen server style)**: the host's PC is both a player and the relay. There's no dedicated server (once headless simulation exists, a process acting only as host is possible).
- **Deterministic input lockstep** (not server-authoritative): every peer runs the same simulation and exchanges only 8 bytes of player input per tick. The host collects, confirms, and distributes inputs and catches desyncs with state hashes.
- **UDP** (no TCP): instead of retransmission, redundant sending that re-includes unacknowledged inputs in every packet, packets of 1200 bytes or less. With Steam, Steam networking relays (NAT traversal, no port forwarding needed).
- **Suitable genres**: co-op, party, RTS, physics games, small-scale versus with 8 or fewer. Bandwidth is constant even with thousands of objects (about 5 KB/s for 4 players), and state-tampering cheats (speed hacks, etc.) show up as desyncs.
- **Unsuitable genres**: competitive FPS, large player counts. Every peer holds the full state so map hacks can't be prevented, there's input delay (60~270 ms depending on ping; my character is drawn moving ahead), and the maximum is 8 players (`kMaxPlayers`). If the host leaves, the match ends (stage 3 host migration).

**What's missing that blocks you right away when making a game**: textures, materials, shadows (stage 2), mid-match joining/reconnecting (stage 3).

---

## Stage 0 (v0.4): developer experience — done

Make the tools comfortable before stacking more features. Principle: **what, where, and how much are handled in the editor (data); how it behaves is handled in code**. No visual scripting.
Per-item implementation details and what's left are in `docs/CHANGELOG.md` "v0.4".

- [x] Godot-style general-purpose editor + game DLL hot reload
- [x] Game code structure: `sim/` · `view/` folders and the determinism check
- [x] Basic tools: `Pool<T>` · tick events · tick timers · `GameContext`
- [x] Game-defined object types + inspector fields
- [x] Tuning values as data
- [x] Declaring state fields only once
- [x] Deterministic math utils (trig approximations)
- [x] Separating players and characters (possession)
- [x] Scene tree hierarchy (parent/child)
- [x] Multiplayer play in the editor
- [x] Instant desync detection with shadow simulation
- [x] Pause, single-tick stepping, rewind
- [x] Periodic autosave
- [x] One-shot verification script (`tools/check.ps1`)
- [x] Project upgrade tool + engine version
- [x] Unified code formatting (`.clang-format`, `.editorconfig`)

**Small things left from stage 0** (done alongside related work):
- With a debugger attached, the PDB gets locked and hot reload builds can fail (consider renaming the PDB per build). There's no editor configuration for the debug CRT (EditorDebug).
- The Pool deletion path was verified only with unit tests. Re-verify with `mp_test` when sample code first deletes.
- Once headless simulation (stage 1) exists, extend the parts of `check.ps1` that run without a GPU.

---

## Stage 1 (v0.5): minimum features to make a game — done (2026-10-19)

Game screen UI part 3, left below as `[ ]`, doesn't block v0.5, so it'll be done when a designer appears. Scenario tests for game code moved to the very start of stage 4 (2026-10-28).

Features that must not be missing to actually ship a game. Most are independent of rendering/simulation, so determinism risk is low. Order:

1. **Risk checks** (a day or two): minimum spec hardware, cross-PC determinism (+ clang-cl comparison), real Steam verification (+ wrapping in an online service layer). A failure would change the stage 2 graphics plan and the `docs/DECISIONS.md` deferred table, so these come before anything.
2. **Platform layer → headless → CI**: gather OS code into one place before audio, input, gamepads, and UI images choose OS APIs. Too late and those items get rewritten. On top of that, set up headless mode and CI so all later work gets determinism checks across compilers, OSes, and CPUs (x64, ARM64). Determinism is the hardest to fix later, so checks are put in place before porting. The license is decided at this point.
3. **Separating data and code**: view tuning → folder layout → sample cleanup → definition data tables. The sample (sandbox) is the template for new projects, so if numbers remain in the sample's code, every project copies them.
4. **The rest in the order below**: since games aren't built together, this document sets the order. An item counts as done only when actually used in a reference sample.

- [x] Project settings (`mealkit.project` via field declarations) — v0.4.1, details in `docs/CHANGELOG.md`
- [x] Godot-style project manager + editor Korean/English + game exe icon — v0.4.2, details in `docs/CHANGELOG.md`

- [x] Minimum spec hardware measurement and cross-PC determinism — v0.4.3, GPU checks, `--system-info`, clang-cl comparison. **Hardware and Intel↔AMD tests weren't run and are assumed passed** (decided 2026-10-18), details in `docs/CHANGELOG.md`

- [x] Real Steam verification — **actual connection tests weren't run and are assumed passed** (decided 2026-10-18), details in `docs/CHANGELOG.md`
- [x] Online service layer (`mealkit/online/`, Steam and none backends) — v0.4.3, details in `docs/CHANGELOG.md`. Achievements, stats, and leaderboards are after v1.0, cloud saves are in the "Saves" item, and enabling several backends together is added with stage 3 EOS

- [x] Platform layer (`mealkit/platform/`, OS implementations in `win32/`, `posix/`, `check_platform.ps1`) — v0.4.3, details in `docs/CHANGELOG.md`. SDL3 evaluation moved to the audio/gamepad items (decided 2026-10-18)

- [x] Headless simulation (`--headless`, `<game>_headless`, `--hash-log`, `--record-inputs`/`--replay-inputs`) — v0.4.3, details in `docs/CHANGELOG.md`. Actual Linux/macOS compiling is in the CI item, and a dedicated relay process that doesn't play is considered together with stage 3 "Host migration"

- [x] CI build matrix (`.github/workflows/ci.yml`: Windows MSVC/Editor/clang-cl, Linux x64/macOS ARM64 Clang headless, `detmath-bits` and fixed input recording per-tick hash comparison) — v0.4.3, details in `docs/CHANGELOG.md`. Windows on ARM after the repo is published (runners), other samples' recordings in the reference samples item

- [x] Engine license: MIT (`LICENSE`) — `docs/DECISIONS.md` 2026-10-16

- [x] Repo publication scope: published when the user requests it, contributions via DCO, sample assets CC0 — `docs/DECISIONS.md` 2026-10-18

- [x] Docs repo (`MealKit-docs`: Sphinx + MyST, English source + Korean gettext, `tools/build.py check`, only `master` before 1.0 and engine MINOR branches from 1.0 — `docs/DECISIONS.md` 2026-10-21) — v0.4.4, details in `docs/CHANGELOG.md`. Opening Read the Docs on repo publication; `--dump-reference` reference generation and Weblate later

- [x] View tuning (`ViewTuning`, `assets/view.tuning`, re-read when the file changes) + time values in seconds (`Ticks`) + color fields (`Color`) — v0.4.5, details in `docs/CHANGELOG.md`
- [x] Enforcing the project folder layout (`tools\check_layout.ps1`: check.ps1, Export, editor; `third_party/<lib>/mealkit.txt` → `tools\gen_notices.ps1`, game libraries and SIM011, exe budget) — v0.4.6, details in `docs/CHANGELOG.md`
- [x] Sample (sandbox) data separation cleanup (numbers in code → `Tuning`, scene area objects, `ViewTuning`; SimCheck float literal warning SIM012; hashes unchanged) — v0.4.7, details in `docs/CHANGELOG.md`
- [x] Definition data tables (`MEALKIT_TABLE`, `assets/tables/*.table`, `RowRef<Row>`, editor [Tables] panel, CSV import/export, simulation tables in the content hash) — v0.4.8, details in `docs/CHANGELOG.md`
- [x] Audio (`mealkit/audio/`: sound events, containers, voice management, buses on top of miniaudio; `assets/sounds.table`, editor [Sound] panel, physics collision events). SDL3 measured and not used — v0.4.9, details in `docs/CHANGELOG.md`
- [x] Events → feedback table (`assets/feedback.table`, `feedback::Player`: sound, camera shake, screen flash, screen freeze; editor [Feedback] panel, `MEALKIT_ENUM`) — v0.4.10, details in `docs/CHANGELOG.md`
- [x] Input action mapping (`mealkit/input/Actions.h`, engine table `assets/input.bindings`, `ctx.actions`, player-changed keys in the user folder) — v0.4.11, details in `docs/CHANGELOG.md`
- [x] Gamepads (`mealkit/platform/Gamepad.h`, Windows XInput, empty slots polled once per second, `pad` bindings in input actions) — v0.4.12, details in `docs/CHANGELOG.md`. Not tested with a real pad
- [x] Confirmed command channel (`LockstepSession::submitCommand`, command bytes in `net::TickInputs`, `mealkit/sim/Commands.h`, input recording version 2) — v0.4.13, details in `docs/CHANGELOG.md`
- [x] Debug UI (Dear ImGui: `GameInstance::debugUi`, F1 overlay in the dev exe, during play in the editor, `MEALKIT_DEBUG_UI` off in shipping builds `build\Export`) — v0.4.14, details in `docs/CHANGELOG.md`
- [x] Localization string tables (`assets/strings/<language>.txt`, `ctx.strings`, default language fallback, build-time font glyph check `mealkit_stringcheck`, font Pretendard) — v0.4.15, details in `docs/CHANGELOG.md`. The Penpot plugin side (attaching keys, exporting tags) is in the game screen UI item
- [x] Game screen UI part 1: `.ui` runtime (`mealkit/ui/`: Flex, constraints, multi-style text, SDF shapes and text, images, widgets, focus, themes, components, lists, world-space UI, one 2D pass) + view models (`MEALKIT_VIEWMODEL`) + editor [UI] preview — v0.4.16, details in `docs/CHANGELOG.md`. Part 2 (Penpot plugin) below
- [x] Settings system + standard settings screen (`settings::Service`, user folder `settings.ini`, `assets/ui/settings.ui`: display, sound, controls, keys, game tabs; 15-second confirmation for display changes, key rebinding) — v0.4.17, details in `docs/CHANGELOG.md`
- [x] Saves — progress data (`game::writeSave`/`readSave`, user folder `saves/<name>.save`, format version/migrate, `.bak`, `GameContext::userFolder`) — v0.4.18, details in `docs/CHANGELOG.md`. Mid-match saves (snapshot files) together with stage 3 mid-match joining; cloud via Steam Auto-Cloud settings and stage 3 EOS
- [x] Match start data — per-player loadouts (`game::setLoadout`/`readLoadout`, sent via lobby signal and confirmed with the start packet, 128 bytes, input recording version 3, `GameInstance::lobby`) — v0.4.19, details in `docs/CHANGELOG.md`

### [x] Reference samples (small examples that verify the engine instead of a game) — sandbox runs title → lobby → match → results → lobby end to end (v0.5.0). The rules below stay in force
- **Why**: games aren't built together in this repo (`docs/DECISIONS.md`). Then there's nowhere to check "it works because it's actually used", and unused features and awkward APIs pile up. The reference samples take that place.
- **Approach**: small samples of different genres go in `samples/<name>/`. All can be opened as projects in the editor and are template candidates (sandbox currently opens via the repo root `mealkit.project`. When there are two samples, each sample folder gets its own `mealkit.project`).
  - `sandbox` (exists now): physics party versus. All stage 1 features (sound, UI, input, settings, saves, screen flow) are used here. v0.5 = sandbox runs title → lobby → match → results → lobby end to end.
  - Later samples are added one at a time when features need them (e.g. RTS with thousands of units — Job System, culling, pathfinding; co-op third person — character movement modes, virtual cameras, skeletal animation). Keep the sample count at 4 or fewer.
- **Rules**:
  - An engine item becomes `[x]` only when actually used in at least one reference sample. Unit tests alone don't finish it.
  - If building a sample gets blocked by a missing engine feature, don't move on with workaround code in the sample; pull that item forward (the stage mark rule in `docs/CODING_MODEL.md`).
  - Every sample passes `check.ps1` (build, `mp_test`, screen capture) and Export. From stage 5, on each platform too.
- **Scope**: samples aren't games. Art, content, and balance only as much as needed to show features.

- [x] Game screen UI part 2: Penpot plugin (`tools/penpot-plugin`) + editor import server + Grid runtime + layout/pixel comparison (`--ui-check`, `--ui-render`, check.ps1) — 2026-10-20, verified in real Penpot (2.18), details in `docs/CHANGELOG.md`

### [ ] Game screen UI part 3: Penpot sources for the default screen templates + component reuse (when a designer appears)
- **Penpot sources for the default screen flow templates** (`art/ui/`): replace the current 8 hand-written screens (title, menu, join, lobby, loading, pause, results, message) and settings/HUD with files redrawn in Penpot and exported. Many screens have lists (`list`) and view models, so together with a designer. Saving Penpot sources doesn't work via the plugin in the current Penpot version, so by hand (`DECISIONS.md` 2026-10-20).
- **Components → reusable `.ui` (`use`)**: currently component instances are flattened on export (the look is the same). When the same button is on several screens, one `.ui` in `ui/components/` + `use`.
- **Checking state variants in real Penpot**: conversion was tested only with fixed data (the sample screens have no variants). Theme variations per token set at that time too.

- [x] Code cleanup (2026-10-19 review: match data in one place, one options struct, splitting editor panels, core include check, join windows via `runGame`, splitting out `ui/Text.cpp`, sharing the join window command line) — 2026-10-20, details in `docs/CHANGELOG.md`

- [x] Default screen flow template (title → menu → lobby → loading → match/pause → results → lobby, engine `game::MatchService` + sandbox `app/Flow`, session rematches, `.ui` text input) — v0.4.24, details in `docs/CHANGELOG.md`
- [x] LAN room discovery (`net/Rooms.h`, room announcements every second during the lobby, room list in the join menu) — v0.4.24
- [x] Lobby/match text chat (`LockstepSession::sendChat`/`chatLog`, host relay and acknowledged resend, lobby screen and Enter during a match) — v0.4.24

- [x] Network condition simulation (`--net-lag`/`--net-jitter`/`--net-loss`, `net/Conditions.h` transport wrapper, `mp_test.ps1 -Lag -Jitter -Loss`) — v0.4.25, details in `docs/CHANGELOG.md`

- [x] Version display and connection refusal reasons (`mealkit.project` `version`, build info header `cmake/BuildInfo.cmake` → `game::BuildInfo`: game version, git commit, simulation version; connection comparison uses determinism group + simulation version + content hash composition, `rejectInfo()`, shipping folder `exportVersionFolder`) — v0.4.31, details in `docs/CHANGELOG.md`

- [x] Player-facing network display (`LockstepSession::pingMs`/`waitingFor`, roster packets during a match, HUD ping and "waiting for P3", lobby ping, setting `gameplay.showNetwork`) — v0.4.26, details in `docs/CHANGELOG.md`

- [x] Automatic input delay adjustment (host picks the start value from lobby pings, changes from the same tick during a match via engine confirmed commands, `mealkit.project` `inputDelayAuto`/`inputDelayMinMs`/`inputDelayMaxMs`, lobby warning) — v0.4.27, details in `docs/CHANGELOG.md`. Per-platform-kind defaults/caps from real measurements on each platform in stage 5; a game setting blocking peers above the cap when needed

- [x] Predicting only my character's appearance (`Simulation::forEachPendingInput`, sandbox `view/Prediction`: draws my character, camera, and name tag ahead by my not-yet-executed movement, `view.tuning` `prediction`) — v0.4.28, details in `docs/CHANGELOG.md`

- [x] Basic accessibility bundle (settings `gameplay.shakeScale`/`flashScale`/`hitStop` → `feedback::Player::setAccessibility`, sound table `subtitle`/`subtitleSeconds` → `audio::Engine::subtitles` → standard `ui/subtitles.ui`) — v0.4.29, details in `docs/CHANGELOG.md`. Color blindness correction via the post-processing LUT (setting `gameplay.colorBlind`, 2026-10-27)

- [x] Tick rate as a project setting (`mealkit.project` `tickRate` 30/60, content hash, `game::applyTickRate`/`tickDt`/`msToTicks`, `--delay <ms>`, sandbox `World::tickRate`/`tickDt`) — v0.4.30, details in `docs/CHANGELOG.md`

---

## Stage 2 (v0.6): looking good (graphics) — done

Everything goes in as **a toggleable option**, with the integrated graphics preset as the default.
**Every graphics item finishes on both the DX12 and Vulkan backends** ("RHI layer" below). An item done on only one side isn't `[x]`.

**Production principle** (`docs/CODING_MODEL.md` "How to make graphics content"): graphics content is **made visually in external tools, and the engine only reads standard formats and draws them.**
No authoring tools (material editor, shader graph, particle editor) are built inside the engine. The editor only does placement, preview, and supported-range warnings.
So in this stage, **fast external tool → engine turnaround** matters as much as features ("Asset hot reload" below is pulled forward).

| Content | External tool (source) | What the engine reads | What's done in the editor |
|---|---|---|---|
| Models, materials | Blender + MealKit add-on | glTF (PBR metallic-roughness supported range + `extras`) | Placement, out-of-range/over-budget warnings, look dev preview |
| Sky, reflections | HDRI (CC0, etc.) | `.hdr` → ambient light/cubemap cache, reflection probes | Environment panel, probe baking |
| Textures | Substance Painter, Krita, Blender, etc. | PNGs inside glTF → BC7/BC5 cache | Preview |
| Animation | Blender | glTF animations | Playback preview |
| Cutscene cameras (stage 4) | Blender | glTF camera animations | Playback preview |
| Effects (particles) | - (a small runtime inside the engine, 2026-10-27) | `assets/effects/*.fx` | [Effects] section of the [Feedback] panel + preview |
| Color grading | Game capture → DaVinci Resolve, Photoshop, etc. | 3D LUT `.cube` | Picking LUTs in the environment panel, post value sliders |
| Shader looks | - (the engine's style presets) | Lighting model in project settings + glTF `extras` features | Preview |
| Impact feedback | - | `feedback.table` | [Feedback] panel |
| Terrain height | Gaea, World Machine, etc. (optional) | 16-bit PNG heightmaps | Refining with brushes |
| Ambient light, sky, fog | - (decided by seeing the scene) | `.scene` environment values | Environment panel (exists now) |

- [x] RHI layer (`mealkit/rhi/`: `Device`, `CommandList`, DX12 + Vulkan 1.3 backends, push constants + bindless array bindings, `build.bat Vulkan`/`VulkanEditor`, DXIL+SPIR-V via pinned DXC, check.ps1 backend capture comparison and validation layers) — 2026-10-20, details in `docs/CHANGELOG.md`. Later stage 2 graphics items finish on both backends together

- [x] Shader hot reload (`render/ShaderReload`: in dev builds, checks `engine/shaders` changes every 0.5 seconds and recompiles with the same DXC and options as the build, `rhi::Device::recreatePipeline`, keeps the previous shader on error, DX12/Vulkan) — 2026-10-21, details in `docs/CHANGELOG.md`

- [x] Benchmark mode + GPU budget split + memory numbers (`--benchmark [seconds]` → `benchmark.txt`, RHI timestamps `beginTimer`/`gpuTimes`/`gpuMemory`, `render::GpuPass`, `mealkit.project` `gpuBudget*Ms`, `FrameSample` 1% worst/GPU/memory) — 2026-10-21, details in `docs/CHANGELOG.md`. Measuring on minimum spec (11th gen) hardware and updating the performance budget table on that PC

- [x] Frustum culling (`render/Culling.h`: mesh bounding spheres + 6 viewProj planes, on the CPU in `DrawList::submit`, `RenderStats::culled`) — 2026-10-21, details in `docs/CHANGELOG.md`. When objects reach tens of thousands, filter by groups first

- [x] Asset hot reload — models and scenes (file times every 0.5 seconds in editor edit mode, `ModelCache::reloadChanged`, scenes changed outside are reopened when there are no local edits, during play after stopping) — 2026-10-21, details in `docs/CHANGELOG.md`
  - Remaining: textures, LUTs, and effect files are added to the same mechanism in their items (textures and materials, post-processing, particles). **Load warnings** (out-of-range material features, more than 64 bones, texture size) in the editor log and inspector with file names — in the material/animation items.

- [x] Blender export add-on (`tools/blender-addon/mealkit`, Blender 5.2 LTS extension: checks and export from the 3D view [MealKit] panel → project `assets/models/<name>.glb`, engine schema `asset::modelSchemaJson` = `schema.json` (unit tests), engine features in the material panel → extras, check.ps1 runs Blender windowless to test) — 2026-10-21, details in `docs/CHANGELOG.md`
  - Remaining: the material feature list (outline, wind, rim, dissolve) is added to the schema with the style presets in "Textures and materials". Asset budget checks use the same schema in the next item, "Asset budget warnings".

- [x] Asset budget warnings (kind = folder under `assets/models/`: `characters`, `props`, `environment`; `mealkit.project` `budget<kind><item>`; warnings on load, inspector usage/limit, Blender add-on checks) — 2026-10-21, details in `docs/CHANGELOG.md`. Texture size and bones start being counted when those features arrive; the budget column in the asset browser (Editor polish) then

- [x] Graphics regression tests (`engine/tests/golden/shots.txt` scenes → `MealKitEditor --render-golden`, comparison with reference images on DX12/Vulkan, diff images `build\check\golden\*-diff.png`, `check.ps1 -UpdateGolden`) — 2026-10-21, details in `docs/CHANGELOG.md`. Post via `post_graded`/`post_off` (2026-10-27)

- [x] GPU capture button and editor view modes (`--gpu-capture`: loads RenderDoc/PIX before the device, [View] > GPU Capture/F11, `rhi/GpuCapture`; [View] > View Mode: wireframe, normals, LOD, overdraw, over budget, `render::DebugView`) — 2026-10-21, details in `docs/CHANGELOG.md`. PIX is code only (unverified); the material feature view when those features exist

- [x] Textures and materials part 1 — texture cache (BC7/BC5 + mipmaps, `build/cache/textures`, bc7enc_rdo), 36-byte vertices (half UV + material index), glTF PBR material table, GGX specular, alpha clip, DX12/Vulkan — 2026-10-21, details in `docs/CHANGELOG.md`

- [x] Textures and materials part 2 — style preset shaders (lighting models Pbr/Toon/Unlit + toon ramp, material features outline/wind/rim/dissolve (glTF extras), instance flash/dissolve progress (flags), material feature view mode, DX12/Vulkan) — 2026-10-26, details in `docs/CHANGELOG.md`


- [x] Textures and materials part 3 — look dev preview ([View] > [Look Dev]: the selected object's model on a turntable + four lighting presets, switching lighting model/flash/dissolve, RHI offscreen target DX12/Vulkan) — 2026-10-26, details in `docs/CHANGELOG.md`. HDRI goes into the same picker in the next item

- [x] Sky HDRI and ambient light (scene environment `hdri`, intensity, rotation; baked on the CPU on load: sky cube + GGX reflection cube 128² 6 mips + SH9 → `build/cache/environments`, RHI cube textures/RGB9E5, skybox, editor [Environment] and look dev pickers, sample Poly Haven CC0 1k) — 2026-10-26, details in `docs/CHANGELOG.md`

- [x] Reflection probes (scene object `reflection_probe` = influence box, editor [Bake]/`--bake-probes`: draws six cube faces offscreen and reads them back (`rhi::Capture::source`), CPU GGX mips → `assets/probes/*.mkprobe`; objects use the one probe whose center is nearest among boxes containing their position; project settings `probeMaxCount` 8, `probeSize` 128; DX12/Vulkan) — 2026-10-27, details in `docs/CHANGELOG.md`. Captured at 8 bits, so light brighter than 1 is clipped (HDR targets in post-processing)

- [x] Point lights (scene object `point_light`: color, intensity, range fields; brief view-side lights are added to `FrameParams::lights`. `drawInstances` picks on the CPU, per instance, up to 4 of the strongest lights touching its bounding sphere (project setting `pointLightsPerObject` 1~8) as 8-byte indices; the shader uses windowed inverse-square falloff + Lambert/GGX (ramp for toon), no shadows, 255 per frame, DX12/Vulkan) — 2026-10-27, details in `docs/CHANGELOG.md`. Hundreds of lights is Forward+ in the deferred table

- [x] Shadows (sun CSM 1~2 cascades, 1024² per cascade, 40 m, texel-snapped sphere + orthographic, depth-only target `rhi::Device::createDepthTarget`/`Target::Shadow`, `DrawList::submit` does the shadow pass before the main view, normal offset + four-Gather PCF, project settings `shadows`/`shadowCascades`/`shadowMapSize`/`shadowDistance`, DX12/Vulkan) — 2026-10-27, details in `docs/CHANGELOG.md`. 0.36 ms on Core Ultra 5 (budget 1.5 ms); minimum spec hardware measurement on that PC

- [x] Skeletal animation (glTF first skin + animations → clips baked at 30 Hz in `asset/Animation.h`, 64 joints, GPU skinning with 44-byte vertices + `SKINNED=1` shader variant, `DrawList::addSkinned` one draw call per character, visual `samplePose`/`blendPoses`/`skinPalette`, simulation `samplePoseTick` (tick → integer frame, included in detmath-bits), scene field `anim` and inspector [Animation], sandbox Pawn idle/run crossfade, Blender add-on skins/actions, DX12/Vulkan) — 2026-10-27, details in `docs/CHANGELOG.md`

- [x] Particles (own runtime after measuring Effekseer: `assets/effects/<name>.fx` = `feedback::Effect`, feedback table `effect` column → `feedback::Player` updates on the view-side CPU, `render::Particle` 32-byte camera quads in one draw call after opaque 3D (no depth writes, premultiplied alpha covering normal and additive, GPU pass Vfx), [Effects] section of the [Feedback] panel + effect preview, sandbox push sparks and bounce dust, DX12/Vulkan) — 2026-10-27, details in `docs/CHANGELOG.md`
- [x] Post-processing (HDR scene target R11G11B10F + bloom dual filter + one uber pass: FXAA, chromatic aberration, exposure, AgX, LUT, vignette, grain; `.cube` → 32³ strip + layered color blindness correction; scene `post` field, [Environment] panel, capture for grading; turning effects off in settings; DX12/Vulkan) — 2026-10-27, details in `docs/CHANGELOG.md`. FSR 1 in dynamic resolution

- [x] Dynamic resolution (main view 3D is drawn into a reduced region of the HDR target and the uber pass upscales with bilinear + contrast-adaptive sharpening, UI at native resolution. A controller lowers the scale when GPU frame time exceeds the sum of GPU budgets; project settings dynamicResolution, minimum scale, sharpening; the player render scale = the upper limit; `--render-scale`; DX12/Vulkan) — 2026-10-27, details in `docs/CHANGELOG.md`

---

## Editor polish (v0.6.x, before stage 3)

Features grew through stage 2, but the editor is awkward to use (2026-10-28 review of 1600×900 captures). Polish it first, before viewing scenes, replays, and desyncs in the editor in stages 3·4 (decided with the user 2026-10-28, pulling stage 4 "Editor/debugging conveniences" forward).
All of it is editor-only, unrelated to determinism and the shipping exe. It doesn't finish a stage, so the version goes to 0.6.x. Done in the order below: earlier items make room (the bottom strip, command table, toolbar) for later ones.

- [x] Redividing the default layout (right column = inspector + [State]/[Tuning] tabs, bottom horizontal strip = build log/tables, [View] > [Layout] edit/play/look dev, the pinned "Scene" row at the top of the scene tree → inspector instead of an [Environment] window) — 2026-10-28, details in `docs/CHANGELOG.md`

- [x] Log console window ([Build] > [Log], a tab in the bottom strip, info/warning/error filters and counts, search, clear) — 2026-10-28, details in `docs/CHANGELOG.md`

- [x] Notifications and in-play indicators (notifications at the viewport's bottom right; failures stay and clicking opens the build log/log; build progress bar; orange border and menu bar during play; guidance when the game DLL is missing) — 2026-10-28, details in `docs/CHANGELOG.md`

- [x] Command palette `Ctrl+P` (one command table shared by menus, shortcuts, and the palette; finding commands, objects, scenes, and tools by Korean/English names) — 2026-10-28, details in `docs/CHANGELOG.md`

- [x] Editor preferences window and F1 shortcut help (camera speed, font size, autosave interval, code editor; window layout per project in `build\editor\layout.ini`; F1 = shortcut list from the command table) — 2026-10-28, details in `docs/CHANGELOG.md`

- [x] Scene tree search and organization (search box `Ctrl+F`, kind icons, per-row visibility/lock → `build\editor\<scene>.view`, expanding and scrolling to the selected object) — 2026-10-28, details in `docs/CHANGELOG.md`

- [x] Multi-select (Shift click, viewport drag box, `Ctrl+A`; moving/rotating/scaling together with gizmos; duplicating/deleting together) — 2026-10-28, details in `docs/CHANGELOG.md`

- [x] Viewport toolbar and orientation indicator (top left gizmo, axis, snap, grid, view mode, camera speed; top right axis orientation indicator; floor grid; gizmo buttons removed from the menu bar) — 2026-10-28, details in `docs/CHANGELOG.md`

- [x] Snap/align settings (move/rotate/scale snap units → preferences and toolbar, grid spacing, drop to floor `End`, align X/Y/Z, even spacing) — 2026-10-28, details in `docs/CHANGELOG.md`

- [x] Camera control improvements (`Alt`+left drag orbit, wheel zooms toward where the mouse points, perspective/orthographic, camera bookmarks `Ctrl+1`~`9`/`1`~`9`) — 2026-10-28, details in `docs/CHANGELOG.md`

- [x] Inspector conveniences (collapsing sections; right-clicking a field for reset to default/copy/paste; a dot on fields differing from the default; locking scale ratio; field descriptions `"name|description"`; [Edit] > [History] `Ctrl+H`) — 2026-10-28, details in `docs/CHANGELOG.md`

- [x] Performance panel and budget warnings ([Build] > [Performance]: current values, budgets, and last 30 seconds graphs for frame CPU/GPU, simulation tick, draw calls, network send; red when exceeded, status bar fps red too) — 2026-10-28, details in `docs/CHANGELOG.md`

- [x] Jumping from errors/objects to code (clicking `file(line)`/`file:line` in the build log/[Log] → preferences code editor `-g`, inspector [Open Code] → the `MEALKIT_OBJECT` declaration) — 2026-10-28, details in `docs/CHANGELOG.md`

- [x] Debug draw API (`debug::line`, `box`, `text`, `debug/DebugDraw.h`; from sim/ too it only queues requests; empty inlines in shipping/headless) — 2026-10-28, details in `docs/CHANGELOG.md`

- [x] Handling multiple scenes ([File] > [Scene List] window: scenes in `assets/scenes`, marking the start scene ★ → `mealkit.project`, opening, [File] > [Recent Scenes] per project) — 2026-10-28, details in `docs/CHANGELOG.md`

- [x] Play from here (`Shift+F5`/[Build] > [Play From Here]; when solo, moves the spawn point to the floor under the camera or the selected spawn, restoring it on stop) — 2026-10-28, details in `docs/CHANGELOG.md`

- [x] Editor asset browser ([Assets] window: `assets/` folder tree and files, dragging models into the scene view, renaming/moving with references fixed together, a list of this scene's broken model references) — 2026-10-28, details in `docs/CHANGELOG.md`

- [x] UX pass (left-hand value names, neutral sections, one-row scene tree tools, viewport toolbar that wraps, play options drop-down, all panels under [View]) — 2026-11-04, details in `docs/CHANGELOG.md`

## Stage 3 (v0.7): making multiplayer solid — done (2026-11-01, v0.7.0)

The actual order was replays → desync diagnosis → mid-match joining → bug reports → host migration → EOS part 1. **Remaining order (re-decided 2026-10-28)**: ~~crash reports (pulled from stage 4)~~ → v0.7.
The rest of EOS needs two PCs and Epic portal setup (Steam provider) (two processes on the same PC share a device ID), so it's done when the user is ready and doesn't block v0.7. Local multiplayer on one PC when the first game genre fits.

- [x] Saving replays (input recording per match in the user folder `replays/`, latest `keepReplays`, game `--replay` and editor [File] > [Open Replay], `LockstepSession::startReplay`, warns on different data and stops at the diverging tick) — 2026-10-28, details in `docs/CHANGELOG.md`

- [x] Desync diagnosis tool (3 snapshots in multiplayer matches; on a desync each peer re-runs to the diverging tick and dumps state fields/bodies to `build/desync/` (user folder in shipping builds); `tools\desync_diff.ps1` finds the first differing tick/field/body; tests `--desync-test`/`mp_test -DesyncTest`) — 2026-10-28, details in `docs/CHANGELOG.md`

- [x] Mid-match joining and reconnecting (the match continues on disconnect, same token/name gets that slot, newcomers get empty slots, host snapshot LZ4 transfer + fast-forward, `sim::readPresence`, `mp_test -LateJoin`) — v0.5.4, details in `docs/CHANGELOG.md`

- [x] Bug report bundles (F9 → user folder `bugreports/<time>.zip`: input recording, info, recent logs, screenshot, one-line note; editor [Open Replay] plays the zip up to that tick and stops; project setting `bugReports`) — 2026-10-28, details in `docs/CHANGELOG.md`

- [x] Host migration (host sends a succession list every second; normal leave `Handoff` / 3 seconds of silence during a match → the next connected peer becomes host, receives further-ahead confirmed ticks via `TickFill` and appends them, old host gets a disconnect `Presence`, Steam lobby owner aligned, `mp_test -HostLeave`) — 2026-10-28, details in `docs/CHANGELOG.md`

- [x] Crash reports (unhandled exception → user folder `crash/pending/` minidump + info + log; the next run bundles `crash/<time>.zip` with the input recording, asks to open the folder; editor [Open Replay] plays to the crash tick; `--crash-at`) — 2026-11-01, details in `docs/CHANGELOG.md`

### [ ] EOS backend (crossplay lobbies/relays) — code part 1 done, remaining measurements don't block v0.7
- **Why**: crossplay needs platform-common lobbies/P2P (online service layer feature table, `docs/CHANGELOG.md` v0.4.3). Verifying on Windows first before stage 5 porting keeps network problems and platform problems from mixing when porting.
- **Approach**: log in with a Steam account (or device ID) via EOS Connect to get a Product User ID, create rooms with EOS lobbies, and use EOS P2P as a `net::Transport`. Steam friend invites carry the EOS lobby id.
  With crossplay on, match transport is EOS only (Steam relays and EOS don't mix in one match).
- **To check**: the EOS P2P packet size limit (if smaller than the current engine limit of 1200 bytes, lower the engine limit), relay RTT, whether 4-player bandwidth is within budget (10 KB/s), host migration and lobby owner changes.
- **Cost**: Epic developer portal registration (free). The SDK DLL is outside the exe budget but counts toward the shipping folder size.
- **Determinism**: no effect (an EOS transport scenario in `mp_test`).
- **Progress (2026-10-28)**: part 1 done — `OnlineEOS.cpp` (device ID login, lobby codes, P2P), `MEALKIT_ONLINE`/`build.bat EOS`, engine packet limit 1170 bytes. Steam account login and Steam invites (carrying the code; device ID before portal setup). Remaining (all need two PCs and portal setup, when the user is ready): joining from two PCs and measuring RTT/bandwidth, lobby owner on host migration, verifying Steam login/invites with the portal's Steam provider, `mp_test` EOS scenario. Details in `docs/CHANGELOG.md`

### [ ] External library review follow-ups (reviewed 2026-10-29) — doesn't block v0.7
- **Review result**: looked at the engine's 17 `third_party/` libraries and MealKit-Audio/Manager's SDL3 and Dear ImGui, down to usage sites and build settings. No library needs replacing or removing. Candidates for review are in the `docs/DECISIONS.md` deferred table (the 2026-10-29 review rows).
- **miniaudio security fix** — done 2026-11-01 (upstream commit `1df46ae` in both repos): 0.11.25 has CVE-2026-32837 (a heap out-of-bounds read when reading WAV BEXT metadata; a crafted WAV crashes). The engine uses WAV decoding (no `MA_NO_WAV` in `MiniaudioConfig.h`), so it's affected. 0.11.26 isn't out yet, so put the upstream fix commit into both the engine's `third_party/miniaudio/` and MealKit-Audio's `third_party/miniaudio/` together and record the commit in the `mealkit.txt` version. The risk is low when reading only our own assets, but it becomes a problem once module/user files are read.
- **stb_image**: 2.30's GIF decoder CVEs (CVE-2026-5185 etc.) don't apply since GIF is excluded via `STBI_ONLY_PNG/JPEG/HDR` (`ui/StbImpl.cpp`). Keep this setting, and when adding a format, check that format's CVEs first.
- **External library rules in tool repos**: MealKit-Audio and Manager have no `mealkit.txt` or generated notices, only a README table. Align them with the engine's `mealkit.txt` + generation approach (there's one contract). The full SDL3 source (33 MB) lives separately in both repos, so see whether only what the build needs can be kept.
- **Line endings**: the ImGui backends in the two tool repos differ only in line endings (LF/CRLF). Pin `third_party/` with `.gitattributes` so diffs are clean when bumping versions.
- **New version/CVE check tool**: right now a person has to look things up. Consider a script (`tools\check_third_party.ps1`; it needs the network, so it's run by hand, not in `check.ps1`) that queries upstream latest tags and OSV (osv.dev) vulnerabilities from `mealkit.txt`'s `source`/`version` and shows them.
- **Determinism**: no effect (miniaudio and stb don't touch the simulation).

---

### [ ] Local multiplayer on one PC (shared screen/split screen) — when the first game genre fits
- **Why**: party games commonly have several gamepads on one PC. Reading 4 gamepads already exists (`Gamepads`).
- **Approach**: one peer provides several player slots (session: local players per peer; tick input stays 8 bytes per player). It can mix with online (two people on each of two PCs). The view is a shared camera or split screen (several viewports, `buildFrame` per player), and UI has per-player focus. Input actions assign devices → local players.
- **Determinism**: only the slot count changes; the tick input format stays the same. A "two players on one peer" scenario in `mp_test`. **Cost**: medium to large (session, local players in `GameContext`, splitting the screen).

## Stage 4 (v0.8): gameplay framework and tools — done (2026-11-02, v0.8.0)

In practice the source module system ①~⑧ was done first (2026-10-28). **Remaining order (re-decided 2026-10-28, same as the item order below)**:
- **Scenario tests to the front** (moved from stage 1): the game flow, health, and movement modes coming next are all rules ("respawn 3 seconds after dying"), better verified with scenarios than unit tests. Module `tests/` also check simulation behavior in the same format.
- **Namespace cleanup to the front**: it's a small job, and putting it off means every project made in the meantime inherits `namespace sandbox`.
- **Game flow framework**, then the gameplay systems in order.
- **Source modules moved out of stage 4 (changed 2026-10-29)**: the module system itself (①~⑧) is done; the remaining modules (⑨) come after stage 5 (cross-platform), added one at a time. See "Modules" below.
- After that, the original order as is. Crash reports sit in the same place as bug report bundles, so they were pulled into stage 3.

### [x] Scenario tests for game code (moved from stage 1) — v0.7.1, details in `docs/CHANGELOG.md`
- **Why**: the biggest advantage of a deterministic engine is that the same input gives the same result. The engine uses this to check `mp_test` and fixed recordings, but game developers have no way to automatically check their own rules ("pushing makes the opponent fall").
- **Approach**: scenario files in the project's `tests/` (scene, seed, input recording or bot, state fields and expected values to check per tick: `tick 300 players[1].falls == 1`). The headless executable runs them with `--test <file>`, and state field names are found via the `MEALKIT_FIELD` declarations (the same paths as the editor [State] panel). A button in editor play saving the current inputs as a scenario; `check.ps1` and project CI run all of `tests/`. A tool that regenerates expected values when simulation results are changed on purpose (like `record_replay.ps1`).
- **Modules**: module repos' `tests/` can use the same format (run by the registry's shared CI `module.yml`).
- **Determinism**: headless as is, so it's also used for cross-platform CI comparison. **Cost**: medium (scenario format + reading field paths + editor button). Recording playback, state viewing, and headless already exist.

### [x] Project namespace cleanup (pulled forward from later) — v0.7.2, details in `docs/CHANGELOG.md`

### [x] Game flow framework (GameMode / GameState) — v0.7.3, details in `docs/CHANGELOG.md`

### [x] Character movement modes (kinematic, physics capsule, floating) — v0.7.4, details in `docs/CHANGELOG.md`

### [x] Active ragdoll movement mode — v0.7.9, details in `docs/CHANGELOG.md`

### [x] Animation state table + animation events — v0.7.5, details in `docs/CHANGELOG.md`

### [x] Virtual cameras (Cinemachine style) + cutscenes — v0.7.6, details in `docs/CHANGELOG.md`

### [x] AI pathfinding (grid / waypoints) — v0.7.7, details in `docs/CHANGELOG.md`

### [x] Prefabs — v0.7.8, details in `docs/CHANGELOG.md`

### [x] Heightmap terrain — v0.7.10 (heights, collision, LOD, brushes) + v0.7.11 (scatter brush), details in `docs/CHANGELOG.md`

### [x] Job System — v0.7.12, details in `docs/CHANGELOG.md`

### [x] Spatial audio and interactive music — v0.7.13 (MealKit-Audio v0.3.0), details in `docs/CHANGELOG.md`

### [x] Asset cooking + pack files — v0.7.14, details in `docs/CHANGELOG.md`

### [x] Performance baseline check — v0.7.15, details in `docs/CHANGELOG.md`

### [ ] Optimization candidates (proposed 2026-10-29) — 1~4 and 9 done, 8 measured and dropped, 5 and 7 measured on the co-op sample and not a cost yet, 5~7 wait for a measured bottleneck and don't block v0.8
- **Why**: the techniques below are not in the engine yet and are not in the `docs/DECISIONS.md` deferred table (GPU-driven rendering, Forward+, mesh shaders, async loading are already there). Each one is small, view-side or build-side, and has **no effect on determinism**. None is done before a measurement shows the cost it removes: each is measured with `--benchmark` before and after (the "Performance baseline check" above), and an item that saves nothing measurable is dropped and moved to the deferred table.
- **Order**: cheap and broad first (1~4), then the ones that need a game or a sample to show the bottleneck (5~9).

| # | Technique | Where | What it saves | Cost / risk |
|---|---|---|---|---|
| 1 | ~~**Front-to-back bucket order**~~ done v0.7.16 | `DrawList::submit` | Sort opaque buckets by the nearest visible instance's distance (and instances inside a bucket roughly by distance) so early depth rejects hidden pixels. Overdraw is the main pixel cost on integrated graphics; the overdraw view mode shows it | A sort of a few hundred keys per frame. Alpha-clip buckets go last (they disable early depth) |
| 2 | ~~**Vertex quantization**~~ done v0.7.19 | `asset::ModelCache`, `Mesh.h`, `Forward.hlsl` | 36-byte vertex → about 20 bytes: position as 16-bit normalized in the mesh box (`meshopt_quantize*`), normal as octahedral 2×16-bit (or 10:10:10). Vertex fetch bandwidth on shared memory, GPU memory | Shader and both backends change; hull points stay from the original float vertices (determinism rule as now). Skinned 44-byte vertices together |
| 3 | ~~**Small-object distance cull**~~ done v0.7.17 | `render/Culling.h`, `ModelCache::pickLod` | Below a screen size (the same measure as LOD) an instance is not drawn at all. A per-kind limit (`props`, `environment`) in project settings; scatter brush instances use it most | Popping; fade out with the existing dissolve flag over a short range |
| 4 | ~~**Pipeline cache on disk**~~ done v0.7.18 | `rhi::Device::createPipeline` | D3D12 `ID3D12PipelineLibrary` / `VkPipelineCache` saved in the user folder, keyed by driver version and shader hash. Shorter first start and fewer hitches on first use of a pipeline (variants grow with skinning, outlines, view modes) | Must ignore a cache from another driver; dev builds keep it off while shaders hot reload |
| 5 | **Static shadow caching** | `Renderer::drawShadows` | Static scene objects are drawn into the cascade once and copied each frame while the sun and the cascade box do not move (texel snapping already keeps the box still while the camera moves within a texel); only moving bodies are drawn on top | One more depth target per cascade. Gain only when static objects are many (terrain, scatter) |
| 6 | **Variable rate shading (tier 1)** | RHI + `Renderer` | Per-draw coarse shading (2×2) for the sky, distant LODs and fog-covered objects. Xe-LP has VRS tier 1 (DX12 `RSSetShadingRate`, Vulkan `VK_KHR_fragment_shading_rate`) | Toggleable option, off where the device lacks it. Checked against golden images with a tolerance |
| 7 | **Animation LOD** | view-side pose update (`samplePose`, `skinPalette`) | Distant or small characters update their pose every 2nd/4th frame, and off-screen characters not at all (the simulation pose path is untouched) | Visible stepping up close; driven by the same screen size as mesh LOD |
| 8 | ~~**Profile-guided optimization for Export**~~ measured 2026-11-02: no gain, moved to the `docs/DECISIONS.md` deferred table | `cmake/MealKit.cmake`, `build.bat Export` | MSVC `/GENPROFILE` build runs `--benchmark` and a headless bot match, then `/USEPROFILE` (LTO is already on). Typically 5~15 % CPU on branchy code (simulation, UI layout); the exe often gets smaller | Export takes longer; profile data per compiler version. Results must stay bit-identical: `mp_test` and detmath checks run on the PGO exe (compilers may not change float results, but it is checked, not assumed) |
| 9 | ~~**Frame allocation check**~~ done v0.7.20 (UI text still allocates about 113 per frame: follow-up) | `check.ps1`, debug builds | Count heap allocations per frame in a steady benchmark frame (a counting `operator new` in dev builds) and warn above 0 for render and UI paths. Keeps "a steady frame allocates nothing" (`DrawList`) true as features grow | Dev builds only; the shipping exe is unchanged |

- **Not proposed here** (already decided elsewhere): Job System (item above, parallel culling/instances/poses), GPU culling and ExecuteIndirect, Forward+, async loading, texture streaming, SIMD in `sim/` (would need determinism verification; Jolt already uses SIMD inside).
- **Determinism**: none of 1~9 touches `sim/`. 8 changes the compiled code of `sim/`, so it is the only one that needs the determinism checks on its output.

### [x] Empty project template — v0.7.21, details in `docs/CHANGELOG.md`. Genre templates wait for 2~3 settled reference samples (deferred table "Genre project templates")

### [x] Second reference sample: co-op third person — v0.7.22 (`samples/coop`, sandbox moved to `samples/sandbox/mealkit.project`), details in `docs/CHANGELOG.md`. Cookbook docs moved to right before contracts are frozen (stage 5, "Cookbook docs")

---

## Stage 5 (v1.x): other platforms, editors on other OSes, crossplay

On top of the platform layer (stage 1) and RHI layer (stage 2, DX12 + Vulkan), add other OS implementations and the remaining graphics backends (Metal, WebGPU, consoles). Simulation determinism has been checked by CI on Linux/macOS since stage 1. The goals are **crossplay where every platform joins one match** and **an editor usable anywhere on Windows, macOS, Linux**.
Each platform gets a minimum spec/performance budget table when started, and finishes only when all reference samples pass Export and `mp_test` (including peers on other platforms) on that platform.

**Platform tiers**:
- **Tier 1**: Windows, Linux/Steam Deck, macOS. Both editor and shipping. v1.0 waits only for Windows; Linux x64 and macOS ARM64 ship in v1.0 as headless builds, and their windows, renderers and editors come in v1.x (`docs/DECISIONS.md` 2026-11-03).
- **Tier 2** (done in order after tier 1): Android/iOS, web, consoles. Shipping only. Consoles can start only once developer registration and dev kits are in place.

| Order | Platform | Graphics | CPU/compiler | Check first | Notes |
|---|---|---|---|---|---|
| 1 | Linux · Steam Deck | Vulkan (exists in stage 2) | x64 · Clang (headless since stage 1) | Whether the Windows exe runs as is on Proton (vkd3d-proton) (during stage 1 real Steam verification). If it does, shipping isn't urgent, but the Linux editor needs native | Steam as is. Case-sensitive file systems |
| 2 | macOS | Metal | ARM64 · Clang (headless since stage 1) | Metal backend, app bundles | Steam as is. Signing/notarization |
| 3 | Android · iOS | Vulkan / Metal | ARM64 · Clang | Mobile GPU budget (tiled GPUs, heat), touch input, ASTC, app lifecycle (what happens to the match when backgrounded?) | No Steam → EOS |
| 4 | Web | WebGPU | wasm · Clang | No UDP (WebRTC data channel transport), threads (SharedArrayBuffer, COOP/COEP), Jolt wasm size | No bindless (WebGPU in `docs/CHANGELOG.md` "RHI layer"), wasm 5 MB budget |
| 5 | Xbox | DX12 (GDK) | x64 · MSVC | The closest. GDK's DX12 differences and certification requirements | Common console items in "Consoles" below |
| 6 | PlayStation 5 | proprietary API | x64 · Clang | A proprietary graphics API backend | |
| 7 | Switch (2) | proprietary API (NVN) or Vulkan | ARM64 · Clang | The weakest GPU, so a separate minimum spec table | |

### [ ] Cross-platform determinism — finishing between real devices (CI checks since stage 1)
- **Why**: lockstep needs every peer to produce the same float results. So far only MSVC/x86 (SSE4.2) is verified. Other platforms differ in compiler (Clang) and CPU (ARM).
- **Approach**:
  - Turn off floating-point contraction in every compiler: MSVC `/fp:precise` (as now), Clang/GCC `-ffp-contract=off`, no `-ffast-math`. ARM has FMA instructions by default, so this option is mandatory. Jolt uses the same options too (keeping `CROSS_PLATFORM_DETERMINISTIC=ON`).
  - No platform branches (`#ifdef`, platform-specific libraries) in simulation code (`sim/`, the engine's `physics`/`DetMath`). SimCheck blocks platform macros in `sim/`.
  - The `detmath-bits` unit test hash must match across all platform builds.
  - A "peer on another platform" mode in `mp_test`: connect two devices with bot input for 900+ ticks. Run via headless simulation (stage 1) without a GPU, leaving per-tick hashes per platform in CI for comparison.
- **Already done**: stage 1 CI compares per-tick hashes of the same input recording on every commit on Windows (MSVC, clang-cl), Linux x64, and macOS ARM64. This item adds on top tests connecting different devices over a real network, plus mobile, web (wasm), and console CPUs/compilers.
- **Until passing**: put a "determinism group" (e.g. `msvc-x64`, `clang-arm64`) into the content hash and refuse connections from peers of other groups. Passing groups are merged into one over time.
- **If it fails**: revisit "Fixed-point gameplay math" in the `docs/DECISIONS.md` deferred table.
- **Progress (2026-11-02)**: done on Windows — SimCheck SIM015 (`#if`/`#ifdef`/`#elif` on platform, compiler or CPU macros in `sim/`; engine `physics`/`DetMath`/`sim` have none), `mp_test.ps1 -Peer host|join -Remote <address>` (one peer per device over the real network, other determinism groups accepted, per-tick hash log `build\mp_test\peer-<group>.hashes.txt`; checked with two processes on one PC: 900 ticks, `desync=0`). Left: running it between real devices (Windows ↔ Linux/Steam Deck, Windows ↔ macOS ARM64) and recording the results here, then merging the passing groups.

### [ ] Crossplay
- **Conditions for the same match**:
  - The determinism group matches (item above).
  - The **simulation version** matches. Exes differ per platform and can't be compared by exe hash, so the game version + a hash of `sim/` and engine simulation sources is baked in at build time and compared (decided in stage 1 "Version display and connection refusal reasons").
  - The content hash is computed only from platform-common data (scene collision data, model physics hulls, tuning, tables). Per-platform cooked results (texture formats, shaders, visual meshes) aren't included.
- **Connection path**: relays differ per platform (Steam relays, console networks) and don't reach each other. Platform-common lobbies/P2P relays are needed → stage 1 "Online service layer", stage 3 "EOS backend". Crossplay matches are always set up via EOS lobbies/P2P.
- **Input differences**: the lobby puts each player's input kind (keyboard/mouse / gamepad / touch) in the match start data and shows it, and whether to mix is a game setting. Per console policy, settings have a "turn off crossplay" option.
- **Determinism**: if the conditions above hold, it's the current lockstep as is. Platform-specific code exists only up to view and input quantization.
- **Progress (2026-11-02)**: done on Windows — the model part of the content hash is the physics hull points (`ModelCache::hashModels`; visual meshes/textures/materials and platform cooking don't count; the committed replays keep their inputs, only the header content hash was rewritten), lobby members carry `net::InputKind` (lobby signal flag bits, shown in the sample lobbies, set from the last used device), project setting `mixInputKinds` off → the host rejects other input kinds (`RejectReason::Input`), player setting `gameplay.crossplay` off → LAN rooms on another OS hidden and joins rejected (`net::platformName`, `RejectReason::Crossplay`). Unit test `net/join_input_kind_crossplay`. Online lobbies (Steam/EOS) have no browsing list, only invites and EOS codes, so joining one from another OS with crossplay off ends at the same `RejectReason::Crossplay`; a lobby attribute filter is needed only if a public lobby browser is added. Left: verifying a Windows ↔ Linux/macOS match over EOS on real devices.

### [ ] Editors on other OSes (macOS, Linux)
- **Depends on**: that OS's platform layer and RHI backend (the editor also draws with that backend).
- **Hot reload**: build game code as `.so`/`.dylib` and load a copy as now (`dlopen`). On macOS, check signed library rules.
- **Engine shared library**: instead of `mealkit.dll`'s MSVC-only exports (`cmake/FixExportsDef.cmake`), Clang achieves the same result with symbol visibility settings.
- **Build scripts**: move `.bat` and Windows PowerShell 5.1 scripts to one that runs on every OS (candidate PowerShell 7 — current `.ps1` files can be used almost as is). The editor's [Build] menu and the project manager call it too.
- **File paths**: asset paths use `/` and exact case. `check_layout` catches references with differing case as errors even on Windows (so they don't first surface on Linux).
- **Exporting for other platforms**: for shipping builds that can't be made on the editor's OS (macOS/iOS only on macOS), decide then on handing off to remote builds or CI.
- **Progress (2026-11-02, v0.8.1)**: done on Windows — build logic moved to `tools/build.ps1` (runs on Windows PowerShell 5.1 and PowerShell 7, DECISIONS 2026-11-02; `build.bat`/`tools\build_project.bat` call it, `build.ps1` in the engine root and project template for Linux/macOS, CI headless builds use it with `Headless`); `check_layout` reports asset references whose case differs from the file; Linux/macOS Editor builds export with default symbol visibility and position-independent code, `platform/posix/DynamicLibrary.cpp` (`dlopen`), game library `<name>_game.dll/.so/.dylib` without the `lib` prefix in `bin/`, editor project paths with `/`. Left: Linux/macOS window/input/process layers and the editor's [Build] menu calling `pwsh build.ps1` there (with posix `Process`), then building the editor on those systems.

### [x] Contract freeze review (v0.9; done 2026-11-03, the contracts are in `docs/CONTRACTS.md`; reviewed 2026-11-03, decisions in `docs/DECISIONS.md` 2026-11-03)
- **What is frozen at v1.0** (the list moves to a `docs/CONTRACTS.md` written by this item, with each format's `@format` version at the freeze):
  - **Game API = source compatibility of the public headers**: a game that builds on v1.0 builds on every v1.x after a rebuild. Binary compatibility isn't promised (the editor already refuses game DLLs with another `kApiVersion`; rebuilding is enough). Today game code (samples, templates) includes 45 of the 124 engine headers; those that stay public are listed, the rest are internal and may change.
  - **Project files**: `mealkit.project`, `.tuning`, `.table` (also `input.bindings`, `feedback.table`), `.scene`, `.prefab`, `.ui`/`theme.ui`, `strings/<language>.txt`, `.fx`, `.scatter`, `.scenario`, tool manifests. Tool-owned formats (`sounds.events` = `audio-events`) follow MealKit-Contract's versioning (`docs/CONTRACT.md` there).
  - **Player-side files** (they outlive game versions on players' PCs): saves (`saves/*.save`), `settings.ini`, the player's `input.bindings`.
  - **`sim/` rules**: the determinism rules in `docs/CODING_MODEL.md` and SimCheck's error rules `SIM001`~`SIM015`.
- **Not frozen**: simulation results across engine versions (a v1.x update, a Jolt version for example, may change them; replays and crossplay already require the same simulation version), replays recorded by another simulation version, cooked `.pak` files, baked probes, desync dumps, engine internals outside the public headers.
- **Already in place**: every text format has `@format <kind> <N>` (no line = 1), readers skip unknown keys of a newer version with a warning and migrate older ones; saves and `.rec` carry a version; the editor checks `kApiVersion`.
- **Work before the freeze**:
  1. [x] Public header list and a check (done 2026-11-03: `engine/public_headers.txt`, 48 headers, `tools\check_public_headers.ps1`, details in `docs/CHANGELOG.md`): headers games may include are listed (a file in `engine/`, read by `tools\check_layers.ps1` or its own script) and samples, templates and source modules fail the check when they include another; the docs reference marks them.
  2. [x] Move the `GameInstance` forwarding into the engine (done 2026-11-03, v0.8.4: `game::SimulationGame<World>`, details in `docs/CHANGELOG.md`): every `GameSession` forwards `nextTick`, `stateHash`, `activeBodies`, `finished`, `captureDue`, `takeSimMsPerTick`, `physics`, `stateView`, `debug` to its `game::Simulation<World>`. The engine provides them (for example a `game::SimulationGame<World>` base that owns the simulation), so the frozen interface keeps only what a game decides. While there, check `lobby()`'s signature against the rest (it takes the session, options and user folder instead of `GameContext`). Template change: samples, `templates/`, `upgrade_project.ps1`, `kApiVersion`, engine version.
  3. [x] (done 2026-11-03: `MEALKIT_GAME_EXPORT` in `game/GameInstance.h`, `__declspec(dllexport)` on Windows, default visibility elsewhere) `MEALKIT_GAME_MODULE` exports with `__declspec(dllexport)`, which Linux/macOS Clang rejects without `-fdeclspec`: an export macro chosen per compiler (the macro's body isn't part of the contract, but the Linux/macOS editor in v1.x needs it).
  4. [x] (done 2026-11-03) `docs/CONTRACTS.md` (the lists above, the format versions, the policies below) and the docs repo page that points to it, English and Korean.
- **Policies after v1.0** (`docs/DECISIONS.md` 2026-11-03): SimCheck rules added in v1.x are warnings only (like SIM012); turning one into an error waits for v2.0. A format change in v1.x only adds keys or a new `@format` version that older files still read into.

### [x] Cookbook docs (done 2026-11-03: docs repo `cookbook/` cooldowns, new input, rounds, event feedback; details in `docs/CHANGELOG.md`. Moved from stage 4 on 2026-11-02)
- Common tasks done while keeping the determinism rules, like "making coins", "timers and rounds", "adding a new input", organized as short examples (docs repo `cookbook/`, English/Korean).
- Written right before v1.0 so the examples show the APIs that get frozen; each example is code from a reference sample, so it breaks visibly when the sample changes.

### [ ] Consoles (Xbox, PlayStation, Switch)
- **Start conditions**: registration in each platform's developer program and dev kits.
- **Code location**: console SDKs and code on top of them are under NDA and aren't put in this repo. Platform layer implementations, RHI backends, and online service backends live in a private repo, and CMake includes them if that path (`MEALKIT_PRIVATE_PLATFORMS`) exists (the same as the Steam SDK Stub approach).
- **Put certification requirements into the design ahead of time** (adding them later means rewriting the app flow):
  - Suspend/resume (the app stops and comes back): in lockstep matches it's treated as a disconnect and comes back via reconnect (stage 3).
  - User sign-in/switching, controller disconnect notices.
  - Saves via platform save APIs (some platforms block writing to the user data folder directly) → stage 1 saves write only through the platform layer's save functions.
  - Achievements, friends, and invites via the online service layer (console platform service backends; crossplay lobbies are EOS).
- **Minimum spec**: Switch gets a separate table (may be lower than the integrated graphics preset).

---

## Modules (after stage 5)

The module system is done (stage 4 ①~⑧). The remaining first modules (⑨) are made after cross-platform (stage 5), **one at a time, whenever the user wants one**. One exception: designer independence D9 pulls `objects` forward.

### [ ] Source module system (feature reuse) — ①~⑧ done, ⑨ remaining modules one at a time
- **Why**: features that differ slightly per game but are used often, like health bars, skill cooldowns, minimaps, inventories, and procedural map generation, are built from scratch every time.
  Putting them in as built-in engine features makes the engine heavier and forces one design. Prefabs hold only data (placement) and can't carry `sim/` logic. Binary plugins (DLLs) don't fit the static single shipping exe or SimCheck.
- **Detailed plan**: `docs/MODULES.md` (when done, rules move to `docs/CODING_MODEL.md`, implementation to `docs/CHANGELOG.md`, and it's deleted). The decision is `docs/DECISIONS.md` 2026-10-28 "Source modules".
- **Approach**: module = a folder **copied as source** into the project's `modules/<name>/` (`module.txt`, `sim/`, `view/`, `assets/<kind>/`, `art/`, `docs/`). `mealkit_add_game()` gathers it like `source/` and includes it in SimCheck, the simulation version, and headless. The installed list is the `modules/` folder itself.
  - Module functions don't know the game's `World`. They take only their own state (`cooldown::step(Cooldowns&, u32 tick)`). The game puts the state in as a `WorldState` field and the tuning as a nested field of `Tuning`, and calls them in order from `World::step()`. If a module's `sim/` includes the game's `source/`, SimCheck fails.
  - Module asset paths are `modules/<name>/<kind>/...`.
  - Adding, updating, and removing is `tools\module.ps1` (called by the editor [Project Settings] > [Modules] and the MealKit Manager [Modules] tab). Updating follows the upgrade tool approach: only unmodified files are replaced, modified files get `.new`.
- **Where they live**: **one module = one git repo** (the first modules at `KingProtea99/module-<name>`, MIT). There are no official modules or official marks. There are no modules in the engine repo (only the copy sandbox uses).
  The registry repo **`MealKit-Modules`**: registration files `entries/<name>.txt` (PR); CI checks them (name, the tag's `module.txt`, empty project build, SimCheck, tests) and builds `index.json` and a search site (GitHub Pages). The shared module CI workflow and the module template live here too. Before an engine release, CI builds all registered modules against the new engine.
- **First modules (KingProtea99 repos, in order)**:

  | Module | sim | view/UI | Notes |
  |---|---|---|---|
  | `cooldown` cooldowns | a bundle of `TickTimer`s | cooldown icon `.ui` | The whole system is verified with this. Moves the sandbox push cooldown |
  | `objects` basic objects | trigger volumes, kinematic platforms moving along paths, item spawn points | none | On top of the stage 0 object type system |
  | `health` health/damage | hit queries (sphere, capsule, ray), damage (attacker, amount, kind), health, invulnerability ticks, death → remove Pawn → respawn wait → possess a new Pawn | health bar and damage number `.ui` | Depends on the game flow framework. Hit results are processed in id order |
  | `procgen` procedural generation | generation from one seed (`Pcg32`, deterministic math) | none | Fits lockstep well (send only the seed instead of the map). Also provides baking into `.scene` in the editor for hand-tweaking |
  | `inventory` inventory | fixed-array slots, item definitions in a definition data table (`.table`) | grid `.ui` | Connected to saves and match start data |
  | `minimap` minimap | none | top-down render or icons | View only |
  | `ai` decision making | utility scoring + `enum` state machines per `Controller`, perception (sight cones, hearing) via physics queries | debug drawing | Uses "AI pathfinding" for movement. No behavior tree graph editor (DECISIONS 2026-10-29) |

- **Docs**: one page, the module repo's `docs/index.md` (English, optional `index.ko.md`) — Overview / How to add (glue code) / Usage / How it works (algorithms, determinism caveats) / Version history. Rendered by the registry site. The docs repo has a one-page guide in "Game developer guide > Modules" + a site link, and module-making rules in "Engine developer guide > Writing modules".
- **Implementation order**: ① engine build integration (`module.txt`, gathering modules, SimCheck rules) → ② asset paths → ③ `tools\module.ps1` (folders, git URLs) → ④ module template + `cooldown` (local) + applying to sandbox → ⑤ registry repo and CI (the user creates GitHub repos and pushes) → ⑥ adding by name, Manager/editor lists → ⑦ site → ⑧ docs repo → ⑨ the remaining official modules. ①~④ are done first without the game flow framework (only `health` waits).
- **Determinism**: module state is a field of the game's `WorldState`, so it goes into hashes and snapshots automatically. Module `sim/` sources go into the simulation version, so peers using different module code are refused connection. Updates in id order.
- **Depends on**: player/character separation, game UI (modules' `.ui`). Only the `health` module needs the game flow framework.
- **Progress (2026-10-28)**: ① done — `mealkit_add_game()` gathers `sim/`/`view/` of folders with `modules/*/module.txt` (include path `modules/`, name/folder match, `requires` and minimum version check, warning if the engine MINOR differs, module `sim/` in headless and the simulation version), SimCheck `--modules`: SIM013 (a module includes the game's `source/`), SIM014 (a module not in `requires`). ② done — asset path → file resolution in one place, `assetFilePath`/`findAssetFile` (`platform/Paths.h`), `modules/<name>/<kind>/...` → `modules/<name>/assets/<kind>/...` (tables, models, UI, sound, scenes, toon ramps, editor [Tables]); the content hash is data read via the same paths, so it's included as is. `check_layout.ps1` checks module folders (module.txt, name, top-level entries, module `assets/` kind rules). ③ done — `tools\module.ps1` list/add/update/remove (source is a folder or git URL`#tag`, `-Name` for repos with several modules, install record `module.files`, updating follows upgrade tool rules: modified files get `.new` and removed files are deleted if unmodified, removal refuses if there are `requires` or `source/` includes), test `check.ps1` "Module tool". ④ done — `cooldown` 0.1.0 (repo `module-cooldown`, local, not pushed yet): `Cooldown`, `tryUse`, `readiness`, component `cooldown_bar.ui`, `tests/` (builds standalone linking the engine core), docs. Sandbox uses the push cooldown and HUD bar via `modules/cooldown` (replays re-recorded). Export ships module `assets/` too, and new projects and `upgrade_project -Source` copy the sample module. ⑤ done — registry `MealKit-Modules` (local, not pushed): `entries/cooldown.txt`, `tools/build_index.py` (checks registration files, verifies each `v*` tag's `module.txt`/`LICENSE`/`docs/index.md`, refuses if an already-published tag moved, `index.json`), `tools/check_module.ps1` (module test build/run + SimCheck, called by the shared workflow `module.yml`), `registry.yml` (registration PR checks/builds, rebuilding `index.json` on main and daily), `template/` + `tools/new_module.py`. One shared CI line in `module-cooldown`. Running on GitHub needs pushing the `MealKit-Modules` repo and tag `v1` and `module-cooldown` tag `v0.1.0`, and module repos must be public. Changed to have no official modules or official marks (DECISIONS 2026-10-28, first modules at `KingProtea99/module-<name>`). ⑥ partly done — `module.ps1 search [query] [-Tag]` (works without a project, this engine MINOR first), `add <name>[@version]` (newest version for this engine MINOR, refused if the tag commit differs from the list, code review notice on add, missing `requires` notice/`-WithRequires`), `update` re-finds registry sources and `-To <version>`, list cache and offline. Tested in `check.ps1` "Module tool" with a local `index.json`. ⑥ done — editor [File] > [Modules...] window (`editor/ModulesPanel`: installed modules, new versions, modified files, registry list, add/update/remove, rebuilds game code on change) and the MealKit Manager [Modules] tab (chosen project), both getting lists with one `module.ps1 search -Tsv`. ⑦ done — registry `web/` (static site, reads only `index.json`): search, tags, engine version filter; module pages render `docs/index.md` at that version's commit (marked + DOMPurify, relative paths to the module repo, language choice if `index.ko.md` exists), copyable add command, version/dependencies/check results/source link. `registry.yml` builds `index.json` then publishes to GitHub Pages (set the Pages source to Actions in repo settings). ⑧ done — docs repo "Manual > Modules" (finding, adding, wiring up, updating, registry site link) and "Working on the engine > Writing modules" (folders, `module.txt`, code rules/SIM013/SIM014, tests/CI, doc template, registration), English/Korean. Next is ⑨, the remaining first modules

---

## Tool suite (runs separately from the stages)

MealKit is a suite with the engine + general-purpose editor at the center, where small tools for workflows that differ per role are built as **separate repos** and tied together by editor shortcuts (`docs/DECISIONS.md` 2026-10-26).
All free, code is MIT. Jobs that already have good free external tools stay external tool + engine plugin (criterion ④ above). Tools are split off **one at a time**, the first being audio. When to start is the user's call, and it doesn't block v0.x stages.

| Tool (working name) | Role | Now | Direction |
|---|---|---|---|
| Audio (runtime + authoring tool) | Sound designers | engine `mealkit/audio/` + `assets/sounds.table` + [Sound] panel | Separate repo. In the order of the items below |
| Game UI runtime (authoring in the editor [UI] panel, Penpot optional) | UI designers | engine `mealkit/ui/` + Penpot plugin `tools/penpot-plugin` | Only the runtime is split off the Audio 1·2 way (UI 1·2 below, `docs/DECISIONS.md` 2026-10-26 "When splitting out subsystems"). No new authoring tool is built (UI 3 below is a preview/check tool, a proposal) |
| Level editing | Level designers | general-purpose editor scene editing | Deeply tied to scenes, Reflect, and the content hash, so it shares the engine core. Boundaries are decided when splitting |
| Data management | Designers | editor [Tables]/[Tuning] panels | For the same reason, boundaries are decided when splitting |
| Game QA | QA | `mp_test`, replays, bug report bundles (stage 3) | The surface touching the engine (replays, logs, repro files) is narrow, so it's easy to split off |
| Code work | Programmers | VS Code, Visual Studio | An own IDE is deferred (`docs/DECISIONS.md` deferred table). Starting with external IDE integration |

- [x] Contract between tools (format line `@format <kind> <N>`, live connection `live::Server` + `build/live/*.json` + `tools\live_probe.ps1`, editor [Tools] menu + manifest `mealkit-tool.txt`) — 2026-10-26 (v0.5.1), details in `docs/CHANGELOG.md`. The local checkout CMake variable for runtime pieces in Audio 2 (`MEALKIT_MKAUDIO_DIR`)

- [x] Audio 1: splitting the runtime layer inside the engine (runtime `engine/libs/mkaudio` (temporary name, `mkaudio::Mixer`, no MealKit headers) + glue `mealkit/audio`, check `tools\check_libs.ps1`, game API and `sounds.table` unchanged) — 2026-10-26, details in `docs/CHANGELOG.md`

- [x] Audio 2: the runtime in a separate repo (MealKit-Audio v0.1.0: deviceless tests + CI (Windows, Linux, macOS), engine `third_party/mkaudio` + `tools\update_mkaudio.ps1`, local checkout `-DMEALKIT_MKAUDIO_DIR`) — 2026-10-26, details in `docs/CHANGELOG.md`. Creating the GitHub repo and pushing is done by the user

- [x] Audio 3: authoring tool (MealKit-Audio v0.2.0 `MealKitAudio`: SDL3 + Dear ImGui, format `assets/sounds.events`, live service `audio/`, `upgrade_project.ps1` migrates `sounds.table`) — 2026-10-26 (v0.5.2), details in `docs/CHANGELOG.md`. After that, spatial audio and interactive music (stage 4 item) are built in the runtime and authoring tool repo

- [x] Contract library: the contract between tools as a separate repo MealKit-Contract (`mkcontract` v0.1.0, spec `docs/CONTRACT.md`), used by the engine (`third_party/mkcontract`, `tools\update_mkcontract.ps1`, `MEALKIT_MKCONTRACT_DIR`), the MealKit Audio tool and MealKit Manager — 2026-11-01, details in `docs/CHANGELOG.md`. Creating the GitHub repo and pushing is done by the user

- [x] Game QA 1: report browser as a separate repo MealKit-QA (`MealKitQA` 0.1.0: bug reports, crash reports and replays of a game in one list, info/log/screenshot/crash text, [Open in Editor] = editor `--replay`, [Copy Info], `--list` without a window; reads only the game's files, mkcontract for the user folder) — 2026-11-02. Creating the GitHub repo and pushing is done by the user. Next candidates: running `tests/*.scenario` with results, turning a report's recording into a scenario, `mp_test` runs

- [x] UI 1: splitting the UI runtime layer inside the engine (runtime `engine/libs/mkui` without MealKit headers + glue `mealkit/ui`, game API and `.ui` format unchanged, steady UI frames allocate nothing) — 2026-11-02 (v0.8.2), details in `docs/CHANGELOG.md`

- [ ] UI 2: the UI runtime in a separate repo (the Audio 2 approach: standalone build, tests, CI; the engine pins a version in `third_party/` + a local checkout CMake variable; whether to move the Penpot plugin too is decided then). Done when another engine/tool wants to use it. Until then, only UI 1's layer split keeps the boundary. Proposal: the Penpot plugin moves with it (designer-facing pieces in one repo, like MealKit Audio's `tool/`), and the earliest time is after the `.ui` format freeze (v1.0), since UI changes together with game features far more often than audio and cross-repo version syncing costs more before that

- [ ] UI 3 (proposal; its preview features moved into the editor [UI] panel editor on 2026-11-04, see "Game UI editor in the editor", so what's left is only a standalone tool for designers without the editor): preview/check tool `MealKitUI` in the UI 2 repo. Not a layout editor (Penpot stays the authoring tool): opens `.ui` files, fills view models with sample data files, switches language/resolution/theme, walks gamepad focus, shows `--ui-check` box differences, and pushes changes to a running game via a live service `ui/` (`reload`, `state`). When a designer appears (with UI part 3), since that's who it serves; until then the editor [UI] panel covers it

- [ ] UI features after v1.0 (one at a time as games need them, each checked against the 11th gen Xe budget): keyframe timeline animation (deferred table, when state transitions aren't enough), virtualized long lists (only visible rows laid out), right-to-left and complex-script shaping (when such a language is added), per-player focus for local multiplayer (with "Local multiplayer on one PC"), UI cost numbers (quads, layout ms) in the benchmark budget

- [ ] Audio features after v1.0 (in MealKit-Audio, runtime + tool together, one at a time): bus effect chains (EQ, compressor/limiter, delay) and sidechain compression in place of volume-only ducking, master limiter + loudness meter (LUFS) in the tool, clip sets loaded/unloaded per scene with a memory budget warning, ~~offline-render golden tests for events~~ (done 2026-11-02, MealKit-Audio 0.4.0), a voice/profiler timeline in the tool over the live `audio/` state, dialogue lines (queue, priority, tied to subtitles), Steam Audio HRTF (deferred table, when headphone positioning is core)
- [ ] Achievements, leaderboards, stats after v1.0 (moved from stage 4 on 2026-11-02, `docs/DECISIONS.md`): reviewed again once the platform SDK work (Steam, EOS, console trophies) is confirmed. Plan as it stood: the game defines achievement conditions as simulation events, the view side reads them and records them through the online service layer, the achievement list is one piece of data exported for each service's registration, and without a service stats are kept in the user data folder. No effect on determinism (only reads results)
- [ ] Opt-in rendering features after v1.0 (`docs/DECISIONS.md` 2026-11-04, one at a time, each with its cost measured on minimum spec hardware and shown to the developer; view-side, so no effect on determinism; additions only, no frozen contract breaks). Shared first piece: project settings for each feature, player quality presets (Low, Medium, High) with a per-game lock, and an editor warning when the enabled features exceed the project GPU budgets. Order: (1) project surface shaders (API and pipeline variant handling designed first), glTF occlusion texture applied to ambient light only, point light shadows on chosen lights (shadow atlas), UI blur, screen-space outlines, mesh particles → (2) post passes that don't run when off: SMAA, EASU upscaling, depth of field, motion blur, SSR → (3) RHI compute on DX12 and Vulkan, then Forward+, SSAO and GPU particles → (4) real-time GI, technique chosen when started. Deferred rendering only when a game shows a need Forward+ can't meet. What a technical artist needs around surface shaders is T6~T9 and T11 in "Technical artist proposals"

---

## Design refresh for the rest of the suite (planned 2026-11-05, not scheduled)

The engine editor's look was improved first (`docs/CHANGELOG.md` "Editor look: fewer lines, less repetition, icons"). The rest will follow; details and order are decided when each one starts.

- [ ] MealKit Manager, MealKit Audio and QA, which still keep their own themes
- [ ] The editor's tree and table arrows, still ImGui's triangles

## Game UI editor in the editor (v0.9.x, decided 2026-11-04)

Every game screen is hand-written `.ui` text because there is no designer, and FuseTag M9 adds many more screens. The editor's [UI] panel becomes a WYSIWYG editor drawn with the real `mkui` runtime (`docs/DECISIONS.md` 2026-11-04). Editor-only: the `.ui` format, the runtime and the game API don't change, so it doesn't touch the frozen contracts and doesn't block publication.

### [x] [UI] panel editing — 2026-11-04, details in `docs/CHANGELOG.md`
- **Why**: placing screens by typing coordinates and reloading is slow, and Penpot's preview isn't what the game draws (line breaks, translations, view model data).
- **Approach**: the panel keeps the `mkui` document open for editing and redraws it every change with the same layout and text code the game uses. Files exported from Penpot (`Document::exported`) open read-only.
  1. [x] `.ui` writer in `mkui` — 2026-11-04, `mkui/Source.h`, details in `docs/CHANGELOG.md`. Was: read → write gives the same text for every `.ui` in the samples and FuseTag, leading comment lines kept. Tree edits (add, delete, move, reorder) rebuild the depth-first `nodes` array.
  2. [x] — 2026-11-04, details in `docs/CHANGELOG.md` (cycle with Ctrl+click, not Alt, which orbits the camera). Was: Selection by clicking the screen (topmost box under the mouse, Alt cycles through boxes beneath) and in a node tree; property window for every `.ui` key (layout, gap, padding, size fill/hug, constraints, fill/stroke/radius/shadow with `$theme` values, text key and style, `button`, `visible`/bindings, `on <state>`); save, undo/redo through the editor's command history.
  3. [x] — 2026-11-04, details in `docs/CHANGELOG.md` (handles show padding; gap isn't drawn). Was: Drag to move and resize: nodes outside a layout change `pos`/`size` (snapping to siblings and the board), nodes inside a flex/grid layout reorder among siblings; handles show padding and gap.
  4. [x] — 2026-11-04, details in `docs/CHANGELOG.md` (values live in `art/ui/samples/`, since `art/` doesn't ship). Was: Preview inputs: sample view model values (a data file next to the screen), language, resolution and UI scale, hover/press/focus states, gamepad focus walk.
  5. [x] — 2026-11-04. Was: Adding nodes (frame, text, image, from a component `.ui`), copy/paste, duplicate, wrap in frame.
- **Checks**: writer round trip over every `.ui` in `check.ps1` (static or editor group); an editor `--ui-edit` script test (open, move a node, save, compare); the game still loads every saved file the same (`--ui-check` boxes unchanged after a round trip).
- **Docs**: docs repo "Manual > Game UI" in English and Korean (placing screens in the panel; Penpot as the optional path).
- **Determinism**: no effect (view side, editor only).

## Editor usability proposals (proposed 2026-11-04, every ★ item done, the rest not scheduled)

A list of candidates from a 2026-11-04 review of the editor code and 1600×900 captures of `samples/sandbox`, with Odin Inspector and Unity editor-enhancer packages as references. Every ★ item is done. **The open items aren't decided**: pick them before work starts, and put open design questions in `docs/DECISIONS.md` first. All of it is editor-only and doesn't change the shipping exe. Per criterion ④, it adds no authoring tools inside the editor (visual scripting, material graphs, particle editors). ★ marks the highest-value items. Parentheses after `[x]` say what a done item left out.

### [x] Check first: problems seen during the review
- [x] The `input.bindings` table showed no rows: details in `docs/CHANGELOG.md`.
- [x] Table column headers used the whole field label: details in `docs/CHANGELOG.md`.
- [x] The bottom strip kept tables to about two rows: tables moved to the [Data] window (item 12).

### [ ] Inspector and field attributes (Odin style)
1. [x] ★ Field attribute syntax: extend the existing `"name|description"` label, for example `"Push radius|Description|group=Pushing|range=0..20|unit=m"`. This may not need changes to `core/Reflect.h`, which would change the simulation version. Check that labels stay out of the content hash and how translation keys are affected before starting.
2. [x] (groups and foldouts; no tabs yet) ★ Groups, foldouts and tabs: `samples/sandbox/sim/Tuning.h` groups its 37 fields only in source comments (Match, Pushing, Respawn/AI), and the panel shows them with no such groups.
3. [x] ★ Ranges and units: sliders, min/max, unit text and a drag speed that fits the value. Every float currently drags at 0.01 (`FieldEditor.cpp`).
4. [x] (searchable lists; no drag and drop or preview button) ★ Asset and event pickers: string fields such as `sound` and `effect` in `feedback.table` are typed by hand. Add a searchable drop-down, drag and drop, and a preview button.
5. [x] Conditional fields: `showIf=teams>0` hides or dims fields that don't apply.
6. [x] (`required` only; no `validate`) Required and validated fields: `required` and `validate` mark the row and report to the Problems panel (item 11).
7. [x] Search inside the inspector and [Tuning]: by name, description, and "only values that differ from the default".
8. [x] List editing (`MEALKIT_LIST`: add, remove, move up; 2026-11-04).
9. [x] ★ Multi-object editing: the inspector currently edits only one object of a multi-selection. Edit shared fields together and show `—` for mixed values.
10. [ ] Pinned and multiple inspectors for comparing objects, and remembering which sections are collapsed.

### [ ] Gathering similar features
11. [x] ★ Problems panel: missing model references ([Assets]), missing row references ([Tables]) and budget warnings (log, inspector) in one list, where clicking goes to the source. Similar in role to Odin Validator.
12. [x] ★ Data hub: [Tuning], [Tables], [Feedback] and the [Sound] preview under one left-hand list, with a list showing unsaved marks in place of the table combo.
13. [x] (input bindings stay a [Data] table) Settings hub: project settings, preferences, input bindings, modules and tools in one window with navigation and search.
14. [x] (a [Debug] window with pages, not a layout preset) Diagnostics layout: log, build log, performance and state opened together as a layout preset.
15. [x] Unsaved documents list: scene, tuning, tables and UI changes in one place on the status bar, each with save and revert.
16. [x] (the menu bar is now [Project], [Scene], [Edit], [Window], [Build] and [Tools]; tuning and state are pages of [Data] and [Debug]) Menu cleanup: [View] lacked Scenes, History, Tuning and State. List every panel in one menu.
17. [x] ★ Shared undo: [Tuning] and [Tables] have no undo, and deleting a row, reloading from file and restoring defaults happen without confirmation. Best done together with item 12.

### [ ] Scene tree
18. [x] (folders; no separators) ★ Folders and separators. A folder saved in `.scene` may affect the content hash, so decide where folders live first.
19. [ ] Color tags, connecting lines and row stripes. Color tags can live in the existing `.view` file.
20. [x] (no prefab badge: objects keep no prefab link) ★ Row badges: prefab, missing model, over budget, differs from default, has a problem.
21. [ ] Tree filters such as `t:spawn`, `is:hidden`, `has:problem`, and "select all of this kind".
22. [ ] Selection history (`Alt+Left/Right`) and favorite objects.
23. [ ] Hover link: hovering a tree row highlights the object in the viewport, and the reverse.

### [ ] Asset browser
24. [x] (kept in memory: three to a few hundred models draw in moments, and showing a cached file needs a texture upload path) ★ Thumbnail grid with a size slider that switches between list and grid, reusing the [Look Dev] offscreen rendering and caching under `build/cache`.
25. [ ] Search, kind filters, favorites, a folder path bar and recently used assets.
26. [x] Scan cache: details in `docs/CHANGELOG.md`.
27. [ ] Preview pane: triangle count, budget bar, sound waveform and a play button.
28. [x] Reverse references: which scenes and tables use this asset, reusing the reference search behind renaming.

### [ ] More visual tools
29. [x] (`gizmo=radius` only, on scene object fields; no `gizmo=point`, tuning values have no place in the scene) ★ Fields linked to the viewport: `gizmo=radius` and `gizmo=point` draw values such as the push radius as spheres and handles that can be dragged.
30. [ ] Viewport icons and labels for spawns, cameras and probes, toggled per kind.
31. [ ] Small preview window for the selected virtual camera.
32. [x] Table visuals: sorting, filtering, color chips, number bars and a row detail pane for long values.
33. [ ] Graphs of [State] fields are done (per frame). Event and desync markers on the play timeline remain.
34. [ ] Tuning A/B presets: switch saved value sets during play, and show differences from the file. This is also designer independence D8; [Debug] > [Balance] already compares two tuning files across bot matches, but not during play.

### [ ] Shared UI and workflow
35. [x] (compact mode and color tokens; toolbar icons not done) ★ Display density and color rules: a compact mode (rows are large at 900p), one set of color tokens for accent, danger, changed and play, and icons with tooltips on the viewport toolbar.
36. [x] ★ Stronger command palette: it only matches substrings today. Add fuzzy matching, Korean initial-consonant search, recent items, and search over assets, table rows and tuning fields.
37. [ ] The scene switcher is done. Rebindable shortcuts remain.
38. [ ] Context hints: replace the fixed status bar text with hints for the current tool and hovered item.

### Suggested order
The original order (groundwork, safe editing, most noticeable, visuals) is done. For what's left:
1. 34 (tuning A/B): also designer independence D8.
2. 25, 27 (asset browser): noticeable as a project's assets grow.
3. 21, 22, 23 (finding things in the scene tree), once scenes grow with the FuseTag maps.
4. 30, 31, 33 (viewport and timeline), then 10, 19, 37, 38 when someone asks.

### Open design questions
Decided 2026-11-04 (`docs/DECISIONS.md`): attributes in the label string, scene tree folders in `.scene` outside the content hash, and the data hub replaces the separate panels.

## Designer independence (decided 2026-11-04, v0.9.x, before publication)

From a 2026-11-04 review of the editor code and FuseTag, aimed at the moments a designer still needs a programmer. The editor panels are already well covered ("Editor usability proposals" above), so these target the remaining places that need a C++ build.

**Engine freeze**: D1~D9 are built now as exceptions to "only fixes until v1.0", including the runtime ones (`docs/DECISIONS.md` 2026-11-04). They may only add keys and new object kinds to frozen formats. Rule rows widen what v1.0 freezes, which is the accepted cost.

**Left over from done items**: D1 wasn't checked on a real PC without Visual Studio or on a network share, and the engine and editor binaries are still copied by hand. D2's columns aren't used by a reference sample or a FuseTag row yet, so a test row on a scratch copy is their only check. D5 doesn't cover replays, and its Accept button hasn't been clicked in a check.

### Most frequent blocks
- [x] D1. Editor without a C++ toolchain (shared folder) — 2026-11-04, details in `docs/CHANGELOG.md`.
- [x] D2. Visual feedback columns (event light, glow kick, hit flash) — 2026-11-04, details in `docs/CHANGELOG.md`.
- [x] D3. `playersFrom` feedback column — 2026-11-04, details in `docs/CHANGELOG.md`.
- [ ] D4. **Squash and spring as data.** `ViewTuning` exposes the numbers, but which event triggers which motion is fixed in FuseTag `view/Juice.cpp`. Move the generic parts into the engine and add feedback columns such as `squash` and `wobble`.

### Workflow breaks
- [x] D5. [Tests] page in the editor — 2026-11-04, details in `docs/CHANGELOG.md`.
- [x] D6. View model field pickers in the [UI] panel — 2026-11-04, details in `docs/CHANGELOG.md`.
- [x] D7. Batch bot matches ([Debug] > [Balance]) — 2026-11-04, details in `docs/CHANGELOG.md`.
- [ ] D8. Tuning A/B presets: item 34 in "Editor usability proposals". List editing, the other half of this item, is done.

### Rules and map gimmicks
The most important area for independence, and the largest. New item effects are fixed in code (FuseTag `ItemEffect` and the `switch` in `sim/Items.cpp`; the table holds only numbers), the M6~M8 map gimmicks need new object kinds and `sim/` code, and the samples' `sim/Objects.h` have no generic trigger or door.
- [ ] D9. **Generic objects, then condition → effect rows.**
  1. Pull the `objects` module forward (trigger volumes, platforms on paths; "Modules" above), with a timer object added.
  2. Add rule rows on trigger objects or tables, such as `player enters → speed ×0.5, 3 s`. Programmers define the list of effects and designers combine them. This keeps the "no visual scripting" principle. Determinism holds as long as the data is in the content hash and the rows run at a fixed point in `sim/`.
  3. Scripting languages stay deferred (`docs/DECISIONS.md` deferred table).
  - **Decide before starting** (`docs/DECISIONS.md`): whether rule rows live on trigger objects or in a table, the first list of effects, and whether FuseTag's gimmicks written as their own scene objects in its roadmap (M6 `Blackout`) move to rows.

### Suggested order
1. D9: next, before FuseTag M6, so the three map gimmicks are built and checked with it.
2. D4: together with FuseTag's character work (M5).
3. D8: any time; it's editor-only.

## Technical artist proposals (proposed 2026-11-04, not scheduled)

Candidates from a 2026-11-04 review of the renderer, the editor and the opt-in plans, aimed at a technical artist (TA) who owns a game's look: writing shaders, checking their cost and keeping the look from breaking. **Nothing here is decided yet**: pick items before work starts, and put open design questions in `docs/DECISIONS.md` first. Sizes are estimates. ★ marks the highest-value items.

**What a TA has today**: style presets, Blender material features, [Look Dev], view modes, GPU capture, `--benchmark` and post values with LUT grading. A TA can't write a shader for a game: `render::ShaderReloader` watches only `engine/shaders`, and "project surface shaders" is the first item of "Opt-in rendering features".

**Principle**: shaders are text edited in an external editor, and looks are checked and tuned in the MealKit editor. This keeps criterion ④ and the deferred rows "Shader graph, material editor inside the engine" and "Own IDE". A node graph is reviewed only if surface functions have been used and TAs who don't write code are blocked by them.

**Engine freeze**: the exceptions in `docs/DECISIONS.md` 2026-11-04 cover the game UI editor and D1~D9, not these. T1~T5 and T10 are editor or tool work or debug-only shader paths, so they fit an exception well, but each still needs a row before v1.0. T6~T9 and T11 build on surface shaders, which come after v1.0 ("Opt-in rendering features"). All are view-side, so none affects determinism.

### Checking a look (no project shaders needed)
- [ ] T1. ★ **More view modes** (S). `render::DebugView` has wireframe, normals, LOD, overdraw, budget and material features. Add base color, roughness, metallic, emissive, sun only, ambient only, reflections only, shadow cascades, mip level and texel density. They extend the `debugView` branches in `PSMain` (`Forward.hlsl`). Those branches are also compiled into the shipping shader today, so the new modes should sit behind a define that only builds with the debug UI set, leaving the shipping shader no larger. Checked on DX12 and Vulkan, like every graphics item.
- [ ] T2. ★ **Pixel inspector** (S). Click the scene view or [Look Dev] to see that pixel's linear HDR value, luminance, value after exposure and final sRGB. Answers "why is this bloom firing" without a GPU capture. Needs a readback of the HDR target on DX12 and Vulkan (`Renderer::captureTarget` reads 8-bit targets today).
- [ ] T3. **[Look Dev] references and comparison** (M). A grey ball, a chrome ball and a color chart beside the model; a split view of two states (lighting model, HDRI, LUT or a saved image), so a change is judged against the same reference every time.
- [ ] T4. ★ **[Materials] page in [Look Dev]** (S). Read-only list of the selected model's materials: factors, textures with size and format, features and their values, the features ignored on load, and which meshes use each one. Clicking a row highlights those triangles. Values stay in Blender, the single source.
- [ ] T5. ★ **Cost comparison** (M). A page like [Debug] > [Balance] that runs `--benchmark` on a scene twice (for example before and after a change, or with a project setting flipped) and shows the difference per `render::GpuPass`. Numbers are from this PC, so the page says which GPU they came from and that minimum spec hardware is the real judge.

### Writing project shaders (with "project surface shaders")
The surface shader item itself stays in "Opt-in rendering features". T6~T8 are the parts of it a TA needs from the first day, so decide them in the same design.
- [ ] T6. ★ **Project shader files and hot reload** (M). `assets/shaders/*.surface.hlsl`, chosen per material from the Blender MealKit panel (a glTF `extras` key, listed in the engine schema). `ShaderReloader` also watches the project folder, and the cook compiles them, so the shipping exe still needs no compiler. Used by water and lava in a reference sample before it counts as done.
- [ ] T7. ★ **Parameters declared in the shader** (M). A block at the top of the file (name, type, range, default, texture) is read at compile time and shown as Blender panel fields and [Look Dev] sliders. Slider changes apply at once. Saving goes to the glTF source through Blender, with one source for each value.
- [ ] T8. ★ **Compile errors where the TA looks** (S). Failed reloads go to [Debug] > [Problems] and a notification, with clicks opening `file:line` in the code editor, and lines mapped from the generated shader back to the surface file. A broken shader keeps the previous one, as now.

### Keeping a look under control
- [ ] T9. ★ **Shader budgets** (M). Project limits on texture samples and estimated instructions per surface shader, from DXC's output, reported in [Debug] > [Problems] and the Blender add-on as warnings, not blocks (`docs/DECISIONS.md` 2026-11-04). A list shows every project shader with its pipeline count, numbers and users. T5 stays the real measurement. Separate from the GPU budget warning in the opt-in features' shared first piece, which judges a whole project's enabled features; both report the same way.
- [ ] T10. **Project look tests** (M). The `--render-golden` comparison run for a game project's own shots (a project `tests/golden/`, `shots.txt`, an update button in [Debug] > [Tests]), on both backends. Catches an engine upgrade or a shader change that alters a game's look. Needs no project shaders, so it works with style presets alone.
- [ ] T11. **Shared library and external editor setup** (S). An engine include with noise, UV and color helpers, documented as part of the surface API, and generated settings for VS Code HLSL extensions (include paths, defines) so completion and error marks work outside the engine folder. Cookbook page with water and lava.

### Later, when a game asks
- T12. Project post effects: one full-screen function in the same form as surface functions, as an opt-in pass with no cost when off.
- T13. Shaders in source modules (`modules/`), so a look can be shared between projects. Needs the surface API frozen first.
- T14. Project shaders for particles (`.fx`) and `.ui` nodes.

### Suggested order
1. T1, T2, T4: small, and useful with the current presets.
2. T5, T3: when a TA is actually comparing looks. T10 fits here if an engine upgrade breaks a game's look.
3. T6, T7, T8 in the surface shader design, then T9, T11.

### Open design questions
- Surface API shape: does it stay one `Surface surf(Input)`, or get a vertex function for waves and wind? The vertex function is hard to take back, as is every public `Input` field.
- TAs without a build toolchain: hot reload and cooking compile with DXC, and `ShaderReloader` turns off on a PC without the engine folder or DXC. DXC arrives with the first engine build, so a PC on the D1 shared folder may have neither. Does the editor bundle or download DXC, and where do the engine includes come from?
- Pipeline count: each surface shader multiplied by lighting model, skinned, shadow and outline pipelines. All are built on load so there are no hitches on first use; how many are acceptable, and what does loading cost?
- Parameter storage: the material record is a fixed 128 bytes, read for every material. Do custom parameters get a separate table in the upload ring?
- Vulkan and later backends: surface files compile to DXIL and SPIR-V like engine shaders; stage 5 conversion (`docs/CHANGELOG.md` "Other backends") must also cover them.

## Roles and tools review (proposed 2026-11-04, not scheduled)

A 2026-11-04 pass over every game development role: how each should work, and whether that work belongs in an external tool or in ours. It applies criterion ④ role by role. **Nothing here is decided yet**: pick items before work starts, and put open design questions in `docs/DECISIONS.md` first. It comes from this roadmap, the decision log, READMEs, file lists and keyword searches, not from running each feature, so check an item's "now" before building on it. The D and T items keep their own sections and are only referenced here.

**Conclusion**: most roles work in external tools and need only a connection from us. Few roles need a large new feature. The real gaps are in the work roles share: collaboration setup, installing the tools, and capture for trailers.

**How to sort a job**:
1. Can the result be judged away from our runtime and simulation? Then it stays in an external tool (modeling, composing, scheduling).
2. Can it be judged only inside the real game? Then it's made externally, and we build the import, preview and checks (textures, materials, animation).
3. Is it tied to our data formats or determinism rules? Then it's done in the editor or a MealKit tool (tuning, scenes, rule data, replays).

### Roles

| Role | How they work | Now | What we'd add |
|---|---|---|---|
| Systems and balance design | Documents in an external tool; numbers in the editor, checked with bot matches | [Tables], [Tuning], [Balance], [Tests] | Ours: D8, D9 |
| Level design | Places and playtests in the editor; rough shapes in Blender | Scenes, terrain brushes, prefabs | Ours: D9 triggers, platforms, timers |
| Narrative | Writes in a dedicated external dialogue tool, then imports | No dialogue system (only the sound item "dialogue lines" in "Audio features after v1.0") | External + module: R4, when a game needs it |
| Gameplay programming | VS Code or Visual Studio, hot reload, SimCheck | Covered | Nothing (own IDE stays deferred) |
| Engine and network programming | RenderDoc/PIX, Tracy, `mp_test`, `desync_diff` | Covered | Nothing |
| Concept art | Krita, Photoshop | No contact with the engine | Nothing |
| 3D modeling and rigging | Blender + MealKit add-on | Export checks, budget warnings | Nothing |
| Texture art | Substance Painter, Krita → glTF | Texture cache, [Look Dev] | Connection: T4 |
| Animation | Blender; playback and state table in the engine | State table, animation events | Connection: IK and root motion when a game asks (deferred table) |
| Technical art | Surface shader functions, diagnosis with view modes | T1~T11, proposals only | Ours: T1, T2, T4, T6~T8 |
| VFX | Effects section of [Feedback] | Own particle runtime | Ours: mesh and GPU particles as opt-ins |
| UI/UX | [UI] panel; Penpot optional | Panel editing done | Nothing (Figma when designers can only use Figma) |
| Sound and music | Reaper, Audacity, a DAW → MealKit Audio | Live connection included | Ours: "Audio features after v1.0" |
| Cinematics | Blender camera animation | No sequencer (decided) | Nothing (revisit under the deferred row's condition) |
| Localization | External translation tool such as Weblate or Crowdin | Per-language string files, font glyph check | Connection: R5 |
| QA | F9 reports → MealKit-QA → scenarios; issues in an external tracker | Covered | Nothing |
| Build and release | CI, Export, store upload tools | D1 shared folder | Connection: R6; D1's check on a real PC |
| Production | Linear, Jira, Notion | Not applicable | Nothing, on purpose |
| Data analysis | External analytics service + replays | No telemetry | Module: R7 |
| Marketing | DaVinci, OBS, store pages | No capture mode found | Ours: R3 |

### Shared gaps
- [ ] R1. ★ **Collaboration setup** (S). `templates/project` has `.gitignore` but no `.gitattributes`, and neither the decision log nor the manual mentions Git LFS or how to handle merge conflicts. Once artists join, `.blend` and PNG files pile up in git history, and two people editing one scene have no documented way to resolve it. Add a `.gitattributes` to the template (bumps `VERSION`) and a manual page in English and Korean. Best done before publication, since new projects copy the template.
- [ ] R2. ★ **Installing without a build** (M). D1 solved getting game code from a shared folder, but the engine and editor binaries are still copied by hand ("Designer independence"). A designer or artist gets stuck on the first day here. The Manager downloads or copies an engine build and registers it. Needs a published engine binary to exist first.
- [ ] R3. **Free camera in replays** (M). Input replays already exist and `scene/Scene.h` already has a player-moved spectator camera mode (`Free`), so this may be mostly connecting them, plus hiding the UI and stepping time. Serves trailers, screenshots and QA reproduction. View-side, so determinism isn't affected.
- [ ] R4. **Where dialogue state lives** (decision first, S). Whether dialogue progress is simulation state or view state decides how it syncs in multiplayer. Record it in `docs/DECISIONS.md` before any module is written. The authoring tool stays external.

### Later, when a game asks
- R5. Translation exchange: convert string files to and from a translation tool's format, and report missing keys and text that overflows its box.
- R6. Store upload script around `steamcmd`, called after Export.
- R7. Analytics module: needs a server, so it stays outside the engine.

### Not to build
- Scheduling, issue tracking and document tools: good external tools exist and they have no contact with the engine.
- Modeling, texture painting and composing tools: we can't catch up with existing tools, and artists want the ones they know.
- Visual scripting and a sequencer: already ruled out, and D9's rule rows cover most of that need.

### Suggested order
1. R1: before publication.
2. D9 → D4 → D8, as already ordered in "Designer independence".
3. T1, T2, T4: small, and needed once art work starts.
4. R3: when FuseTag needs a trailer. R2 once a published engine binary exists.
5. R4 and R5~R7 wait for a game that asks.

**Engine freeze**: the exceptions in `docs/DECISIONS.md` 2026-11-04 don't cover these. R2 is mostly Manager work and R3 should be editor and view-side only, so neither looks likely to touch the frozen contracts, but neither is designed yet and R3 needs a row before v1.0.

### Open design questions
- R1: is Git LFS the default or only a recommendation? Hosting quotas are a cost for free users. Do `.scene` files merge cleanly as text, or do they need a merge rule? This hasn't been tested.
- R2: where do engine binaries come from, and how do they match a project's `engineVersion`?
- R3: where does the camera live during a replay in the shipping exe, the editor, or both? Does a shipped game offer it to players as a photo mode, or is it dev-only?

## Engine module split (decided 2026-11-08, before publication)

The engine becomes a kernel, and every subsystem an engine module in its own repo (`docs/DECISIONS.md` 2026-11-08, design and details in `docs/ENGINE_MODULES.md`). Built now, before "Publication preparation" finishes, since repo layout and contract paths change. Each item is done when the Editor and Release builds, render goldens on DX12 and Vulkan, and the determinism group pass.

- [x] 1. Kernel memory — 2026-11-08, details in `docs/CHANGELOG.md` (miniaudio hooks and per-tag budgets left for later)
- [x] 2. Manifests and `check_modules.ps1` — 2026-11-08, details in `docs/CHANGELOG.md`
- [x] 3. Kernel in `engine/core/` — 2026-11-08, details in `docs/CHANGELOG.md`
- [x] 4. Modules in `modules/<name>/` — 2026-11-08, details in `docs/CHANGELOG.md` (per-module CMake targets left for the repo split; `asset`'s GPU parts moved to `render` 2026-11-09)
- [x] 5. Service registry and module order — 2026-11-08, details in `docs/CHANGELOG.md`
- [x] 6. Session interface — 2026-11-08, details in `docs/CHANGELOG.md`
- [x] 7. Project module lists and presets — 2026-11-08 (engine 0.9.1), details in `docs/CHANGELOG.md` (presets `standard` and `solo`; `kernel`/`headless` as named presets, and the editor refusing a game DLL that includes a module its project left out, wait for per-module libraries in step 9)
- [x] 8. Editor panels into modules' `editor/` folders; online backends as modules — 2026-11-08, details in `docs/CHANGELOG.md`
- [x] 9. Module repos extracted with history, `modules.lock`, `tools\modules.ps1`, shared module workflow — 2026-11-08, details in `docs/CHANGELOG.md`. Left for the user: create the 20 GitHub repos under `MealKit-Engine` and push them with their `v0.1.0` tags, then tag the engine `v1` for the shared workflow. `audio` stays in the engine until its glue moves into `MealKit-Audio`
- [x] 10. Workspace notes and tools, rules, CODING_MODEL, CONTRACTS, docs repo, publication order — 2026-11-08, details in `docs/CHANGELOG.md`
- [x] 11. Engine modules used like source modules: any combination per project, a version per module, layer tags — 2026-11-08, details in `docs/CHANGELOG.md` (`audio` optional since 2026-11-08; `game` still needs the other framework modules: `ui`, `feedback`, `settings`, `input`, `loc`, `render`, `rhi`)
- [x] 12. A game's own engine module copies in its `modules/` folder (`modules.ps1 add -Project`) — 2026-11-09, details in `docs/CHANGELOG.md`
- [x] 13. Engine and source modules handled the same way (`module.txt` `type engine`, `tools\module.ps1`, the game's `modules/` is the selection) — 2026-11-09, details in `docs/CHANGELOG.md`

## QA, optimization and audio, then repository publication (decided 2026-11-02)

Other-OS testing (the rest of stage 5) waits for Linux/macOS devices. Until then the next work is the QA, optimization and audio items below, and the repos are published after them (`docs/DECISIONS.md` 2026-11-02). The remaining designer independence items (D9, D4, D8) are also meant to land before publication (`docs/DECISIONS.md` 2026-11-04).

### [ ] Before publication, in this order
1. [x] **Game QA 2** (MealKit-QA 0.2.0) — v0.8.3, details in `docs/CHANGELOG.md`
2. **Optimization** (stage 4 "Optimization candidates", measured before and after with `--benchmark`): ~~#8 PGO for Export~~ (measured: no gain, deferred table; the #9 follow-up was already done in v0.8.2) → #5 and #7 only if the co-op sample shows the cost (measured 2026-11-02 on `coop --benchmark 15`, hills scene: frame 2.29 ms, GPU-bound at 2.26 ms of the 8 ms budget, shadow pass 0.53 of 1.50 ms, simulation 0.26 ms/tick; skinned figures are one per player (at most 4), so neither is a bottleneck and both keep waiting, unimplemented) → #6 last.
3. [x] **Audio** golden tests (MealKit-Audio 0.4.0: `mkaudio/Golden.h`, `MealKitAudio --golden --project <folder> [--update]` → `tests/audio/<event>.golden`, rms/peak per 10 ms within 1.5 dB) — 2026-11-02. The engine keeps mkaudio 0.3.1 in `third_party/` (the game needs nothing new); the other "Audio features after v1.0" stay there unless a DECISIONS row moves one forward

### [ ] Publication preparation (after the items above; surveyed 2026-11-03)
- [x] **Personal data in history** (2026-11-03, with the user): `git filter-repo` rewrote `MealKit`, `MealKit-Manager`, `MealKit-QA` and `MealKit-docs`: every `baghakdorung`/`yhak0205` author, committer, file text and message became KingProtea99 / kingprotea99@gmail.com. Commit hashes before 2026-11-03 changed (hashes quoted in old docs no longer resolve); the other repos had nothing to rewrite and keep their hashes and tags. Pushing needs `git push --force` once (done by the user). Mirror backups of the old history: `D:\MealKit-Engine-backup-2026-11-03` (delete after the push). No keys/tokens/private keys in any history; no SDK binaries tracked.
- [x] **Agent notes are not published** (with the user): every `CLAUDE.md` except `templates/project/CLAUDE.md` (a template file new game projects get, managed by the upgrade tool) was removed from history. Local copies stay untracked through `.git/info/exclude`. Their contributor rules moved to the engine's `CONTRIBUTING.md` and the Manager/QA READMEs ("Working on it"); the docs point there.
- [x] **Licenses and notices**: `LICENSE` in `MealKit-Modules`; `samples/*/ASSETS.md` (CC0 except the Pretendard font, SIL OFL, and the Poly Haven HDRI source); `.gitattributes` in `MealKit-Modules` and `module-cooldown`.
- [x] **Community files**: engine `CONTRIBUTING.md` (rules, build, checks, DCO) and `SECURITY.md`; org repo `MealKit-Engine/.github` (local folder `D:\MealKit-Engine\.github`, not pushed): profile README, default `CONTRIBUTING.md`, `CODE_OF_CONDUCT.md` (Contributor Covenant 2.1), `SECURITY.md`, issue forms (bug report asking for the F9 zip, feature request, Discussions link), pull request template. The org files cover every org repo without its own; `module-cooldown` (under KingProtea99) has none.
- [x] **README beta notice** in all 8 repos; engine README links CONTRIBUTING/SECURITY.
- [ ] **Sponsorship** (the user is looking into it): `FUNDING.yml` in the org `.github` repo once decided. GitHub Sponsors pays out to South Korea (Stripe, W-8BEN); Open Source Collective needs about 100 stars; taxes to check first.
- [ ] **At publication, on GitHub** (user): create the `.github` repo and push it; create the 20 engine module repos (`modules.lock`) and push each with its `v0.1.0` tag before the engine, then push the engine and tag it `v1` for the shared module workflow (`engine-module.yml@v1`); force-push the four rewritten repos; install the DCO app (checks `Signed-off-by` on pull requests); branch protection on `main` (CI must pass); Discussions on in `MealKit`; private vulnerability reporting, secret scanning and Dependabot alerts on; topics; Pages source Actions for `MealKit-Modules`.
- [ ] **Later, optional**: a screenshot in the engine README (`MealKitEditor --capture`); `module-cooldown` community files.
- [ ] **Last checks**: full `tools\check.ps1`, push the remaining local commits.

### [ ] Publication (done by the user)
- Order follows dependencies: `MealKit-Contract` → `MealKit-Audio` → `MealKit` → `MealKit-docs` → `MealKit-Manager`, `MealKit-QA` → `MealKit-Modules` (tag `v1`, Pages source set to Actions) → `module-cooldown` (tag `v0.1.0`).
- After the first public CI runs: Read the Docs for `MealKit-docs`, the Windows on ARM runner (both waited on publication).
- [x] **First green public CI on Linux x64 and macOS ARM64** (2026-11-03, `MealKit-Engine/MealKit` commit 9dc8a42): unit tests pass on both, `detmath-bits` and the sandbox and coop replay per-tick hashes match Windows MSVC and clang-cl, coop scenarios and headless `mp_test` pass. Details in `docs/CHANGELOG.md`. It is the only other-OS evidence until real devices are available.

## After publication (proposed 2026-11-05, not scheduled)

The repos are public, so what is missing now is mostly what a first visitor sees and can try. The first GitHub release is v1.0.0 (`docs/DECISIONS.md` 2026-11-05).

- [ ] A screenshot or play GIF in the engine README
- [ ] Check that the GitHub settings under "At publication, on GitHub" are really on
- [ ] Full `tools\check.ps1` (the "Last checks" item above)
- [ ] First-run check on a clean Windows PC: build from source, new project, run, export
- [ ] A short demo video: 4 players and 8,000 physics boxes on integrated graphics
- [ ] Get outside feedback before freezing contracts for v1.0, since they are hard to take back afterwards
- [ ] GitHub release v1.0.0 with Windows binaries

## Recommended order summary

0. ~~General-purpose editor + game DLL hot reload~~ → ~~`sim/`/`view/` structure + determinism check~~ → ~~basic tools (Pool, tick events, tick timers, GameContext)~~ → ~~object types + inspector fields~~ → ~~tuning values as data~~ → ~~declaring state fields once~~ → ~~deterministic math utils~~ → ~~player/character separation (possession)~~ → ~~scene tree hierarchy~~ → ~~autosave~~ → ~~multiplayer play~~ → ~~shadow simulation~~ → ~~rewind~~ → ~~verification script~~ → ~~project upgrade + engine version~~ → ~~code formatting~~ **(v0.4 done)**
1. ~~Project settings~~ → ~~project manager, editor language, exe icon~~ → **risk checks** (~~minimum spec hardware, cross-PC determinism, clang-cl comparison~~ → ~~real Steam verification~~ + ~~online service layer~~) → ~~**platform layer** (+ minimal POSIX implementation)~~ → ~~headless simulation (+ input recording playback, Linux/macOS builds)~~ → ~~CI build matrix (hash comparison across OS/CPU)~~ → ~~license (MIT)~~ → ~~publication scope decision~~ → ~~docs repo~~ → **data separation** (view tuning + time values in seconds → enforcing the project folder layout → sample data separation cleanup → definition data tables) → reference sample rules → audio → events → feedback table → input action mapping → gamepads → confirmed command channel → debug UI (ImGui) → localization → game UI (`.ui` runtime + preview panel) → settings system + standard settings screen → saves (progress data) → match start data (loadouts) → ~~code cleanup 1·2 (match data in one place, one options struct)~~ → ~~default screen flow (+ LAN room discovery, text chat)~~ → ~~network simulation~~ → ~~player-facing network display~~ → ~~automatic input delay adjustment~~ → ~~predicting my character's appearance~~ → ~~accessibility basics~~ → ~~tick rate setting~~ → ~~version display and connection refusal reasons~~ **(v0.5 done)**. Not blocking v0.5: scenario tests for game code (moved to the very start of stage 4). ~~Penpot plugin + import server → layout/pixel comparison checks~~. When a designer appears: Penpot sources for the default screen templates
2. ~~RHI layer (DX12 + Vulkan)~~ (later graphics items on both backends together) → ~~benchmark mode (+ GPU budget, memory numbers)~~ → ~~shader hot reload~~ → ~~frustum culling~~ → ~~asset hot reload~~ → ~~Blender add-on~~ → ~~asset budget warnings~~ → ~~graphics regression tests~~ → ~~GPU capture/view modes~~ → ~~textures/materials part 1 (texture cache, glTF PBR)~~ → ~~style presets~~ → ~~look dev~~ → ~~sky HDRI/ambient light~~ → ~~reflection probes~~ → ~~point lights~~ → ~~shadows~~ → ~~skeletal animation (+ pose sampling for the simulation)~~ → ~~particles (own runtime)~~ → ~~post-processing (uber pass + LUT)~~ → ~~dynamic resolution~~ **(v0.6 done)**
- Editor polish (after stage 2, before stage 3): layout → log console → notifications → command palette → preferences/F1 → scene tree search → multi-select → viewport toolbar → snapping → camera controls → inspector conveniences → performance panel → jump to code → debug drawing → multiple scenes → play from here → asset browser **(v0.6.x)**
3. ~~Replays~~ → ~~desync diagnosis~~ → ~~mid-match joining~~ → ~~bug report bundles~~ → ~~host migration~~ → ~~EOS backend part 1~~ → ~~crash reports (pulled from stage 4)~~ **(v0.7)**. Not blocking v0.7: EOS measurements (two PCs, portal setup). If the first game genre fits: local multiplayer on one PC
4. ~~Source module system ①~⑧ (engine integration, `module.ps1`, `cooldown`, registry `MealKit-Modules`, editor/Manager lists, site, docs)~~ → ~~scenario tests for game code (moved from stage 1)~~ → ~~project namespace cleanup~~ → ~~game flow framework~~ → ~~character movement modes (kinematic → floating → active ragdoll)~~ → ~~virtual cameras + cutscenes~~ → ~~AI pathfinding~~ → ~~prefabs~~ → ~~terrain (+ scatter brush)~~ → ~~Job System~~ → ~~spatial audio/interactive music~~ → ~~asset cooking + pack files~~ → ~~performance baseline check~~ → ~~empty project template~~ → second reference sample (co-op third person) **(v0.8)**
4½. While other-OS devices are missing: ~~Game QA 2~~ → ~~optimization (measured)~~ → ~~audio golden tests~~ → engine module split (kernel + one repo per module, `docs/ENGINE_MODULES.md`) → publication preparation → repos made public (user). Alongside (v0.9.x): ~~game UI editor in the [UI] panel~~ → ~~editor usability ★ items~~ → ~~designer independence D1, D2, D3, D5, D6, D7~~ → D9 (before FuseTag M6) → D4 → D8. Not scheduled: the rest of the editor usability items, technical artist T1~T5 and T10 (each needs an exception row before v1.0), roles review R1~R4 (R1 before publication if picked)
5. Contract freeze review → cookbook docs **(v0.9)** → contracts frozen **(v1.0)** → cross-platform determinism (real devices) → Linux/Steam Deck (Vulkan, including editor) → macOS (Metal, including editor) → crossplay **(v1.x)** → tier 2: Android/iOS → web (WebGPU) → consoles (once dev kits exist, Xbox → PlayStation → Switch)
- Opt-in rendering (after v1.0, one at a time): surface shaders with technical artist T6~T9 and T11 → other features in the order of "Opt-in rendering features"
- Modules (after stage 5, one at a time as wanted): source modules ⑨ remaining first modules (basic objects (pulled forward by D9) → health/damage → procedural generation → inventory → minimap → ai)
- Tool suite (separate from stages, start decided by the user): ~~contract between tools~~ → ~~Audio 1 (layer split inside the engine)~~ → ~~Audio 2 (separate repo)~~ → ~~Audio 3 (authoring tool)~~ → ~~contract library~~ → ~~UI 1 (layer split inside the engine)~~ → next tool. UI 2 (separate repo, earliest after v1.0) when a use appears, UI 3 (preview tool) when a designer appears. UI/Audio features after v1.0

What to check when finishing each feature: no Release build warnings, `mp_test.ps1 -Clients 3 -Ticks 900` PASS, the default preset within budget on minimum spec (11th gen Xe) integrated graphics (opt-in features: no GPU cost when off, cost measured when on), actually used in a reference sample, no OS/graphics API calls outside `platform/`/RHI, per-tick hashes match in every CI build, (from stage 2) the same image on the Vulkan backend too, reflected in the docs repo in English and Korean (`tools/build.py check` passes),
and new external libraries get `LICENSE` and `mealkit.txt` in `third_party/<lib>/`, then `third_party/VERSIONS.txt` (the canonical external library table) and `THIRD_PARTY_NOTICES.txt` are regenerated with `tools\gen_notices.ps1`.

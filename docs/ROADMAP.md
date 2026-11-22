# MealKit engine roadmap

**Only work that is left.** A finished item is removed from this document and its details go to `docs/CHANGELOG.md`; a name quoted elsewhere as ROADMAP "…" for a finished item is found there by the same name. Reasons for decisions and the "Deferred" table are in `docs/DECISIONS.md`. When the order changes, update this document.

## Where it stands (v0.9.5, 2026-11-13)

Stages 0~4 are done (v0.4~v0.8), the contract freeze review and the cookbook are done (v0.9.0, `docs/CONTRACTS.md`), the engine is a kernel plus 20 engine module repos (`docs/ENGINE_MODULES.md`), the game UI runtime and its Designer live in MkUI (UI 2a), and the repos are public (first green public CI on Linux x64 and macOS ARM64 2026-11-03). Until v1.0 only fixes go in, plus the decided exceptions that only add to frozen formats (designer independence, the game UI editor; `docs/DECISIONS.md` 2026-11-03 and 2026-11-04).

**Next, in order** (changed 2026-11-22, `docs/DECISIONS.md` "Linux editor and editor reinforcement come first"; game work such as FuseTag M5/M6 waits):
1. Linux editor finish and editor reinforcement, including what the scene-editing move left unfinished ("In progress" first item).
2. 2D on the 3D engine: what is left of the sprite item (the flipbook preview is MkRender's, so it comes with item 1's MkRender work).
3. Designer independence D4 (with FuseTag M5, later).
4. UI 2: MkUI's XAML-like layer, one piece at a time.
5. After publication: the checks and the v1.0.0 release.
6. When devices and accounts are available: EOS measurements, cross-platform determinism and crossplay on real devices, minimum spec measurements → stage 5 (v1.x).

Not scheduled (picked before work starts): the open editor usability items (tables and spreadsheets 39~43 among them), AI agents and game developer ease A1~A7, visual feedback proposals, technical artist T1~T14, roles review R1~R7, optimization #5~#7, modules ⑨, the tool suite rows.

## Decision criteria

Every feature is weighed in the order below to decide whether it goes in.

1. **Does it not hurt 3D + multiplayer?** Features touching the simulation must keep lockstep determinism (`docs/CODING_MODEL.md` "Determinism rules").
2. **Lightness and speed.** The target hardware is office PCs with Intel integrated graphics, **minimum spec Intel 11th gen (Xe integrated graphics) or newer**. The budgets below are what the default preset guarantees, not a limit on what the engine supports: features exceeding them are opt-in (off by default, no GPU cost when off, turned on per project, player-toggleable through quality presets unless the game locks them; `docs/DECISIONS.md` 2026-11-04).
3. **Multiple platforms, DX12 as reference.** OS APIs are called only behind the platform layer, graphics APIs only behind the thin RHI.
   New rendering features are designed on the DX12 backend first, and **the Vulkan backend is built together within the same item**. Metal, WebGPU, and console backends (stage 5) follow. Features aren't lowered to fit other APIs; features a backend can't do are turned off in that platform's preset.
   The editor is Windows first, macOS/Linux in stage 5. The goal is crossplay between all platforms.
4. **Use finished things instead of building them.** The engine repo handles only the deterministic simulation (+ physics glue), lockstep networking, renderer, and editor (placement, import, checks, debugging, tool shortcuts).
   - **External authoring tools** (Blender, Penpot, VS Code, DaVinci, etc.) are used actively. They don't go into the game exe, so they cost nothing. The engine builds only import, checks, previews, and plugins for the tools.
   - **MealKit tools**: built only when no free tool fits a role's workflow. Separate repos outside the engine repo, MIT, one contract between tools ("Tool suite" below, `docs/DECISIONS.md` 2026-10-26).
   - **Runtime libraries** (linked into the exe) are also preferred over building our own. With conditions:
     freely if used only on the view side, only determinism-verified ones if they touch the simulation (like Jolt). Source included, version pinned (`third_party/`), permissive license (MIT/BSD/zlib/public domain, etc.), within the shipping exe budget (5 MB).
     Libraries that run on every target platform are looked at first. One platform's native APIs are used only inside that platform's implementation in the platform layer.

**Networking model** (fixed): host-client lockstep over UDP with input-only exchange (8 bytes per player per tick), host relay, state hashes for desync detection, host migration. Suits co-op, party, RTS, physics games and small versus with 8 or fewer players; doesn't suit competitive FPS or large player counts (every peer holds the full state, input delay 60~270 ms by ping).

### Performance budget of the default preset (minimum spec Intel 11th gen Xe integrated graphics, 1280×720, 60 fps)

"Current" values were measured on a Core Ultra 5 125U (Xe-LPG), faster than the minimum spec. **They must be re-measured on minimum spec hardware** (not done yet).
This table is for Windows. Minimum specs and budgets for other platforms are set in a table of the same shape when that platform is started (stage 5). The 5 MB executable is the same on all platforms.

| Item | Budget | Current |
|---|---|---|
| Whole frame (GPU) | 8 ms or less (half left as headroom at 60 fps) | about 2.2 ms (arena, shadows/post on: opaque 1.33, shadow 0.39, post 0.45, 2026-10-27 `--benchmark 20`) |
| Simulation tick (30 Hz) | 4 ms or less per tick | 0.9~2.5 ms (2,000 bodies) |
| Draw calls | 200 or fewer | 26 (816 instances, arena) |
| Network (4 players, host send) | 10 KB/s or less | about 5 KB/s |
| Game executable | 5 MB or less (excluding assets) | about 3.6 MB (Export build, including audio, Jolt, post, 2026-11-03) |

## Versions

The engine version is `VERSION` at the repo root. **v0.n = the engine having finished stage n.** **Everything before v1.0 is beta**: version numbers only mark progress and promise nothing. v1.0 is when the user declares the engine finished. During 0.x the `GameInstance` API (`kApiVersion`) and file formats may change, and existing projects are migrated with the upgrade tool.

| Version | Meaning |
|---|---|
| v0.9 (now 0.9.5) | v1.0 candidate: contracts reviewed, cookbook written; only fixes and the decided exceptions until v1.0 |
| **v1.0** | **Windows** (editor and shipping, DX12 and Vulkan) is fully supported and **Linux x64 / macOS ARM64 run the headless simulation** (per-tick hashes match Windows in CI on every commit). All reference samples pass Export and the determinism checks on Windows, and **contracts are frozen** (`docs/CONTRACTS.md`) |
| v1.x | Stage 5 tier 1 without breaking the frozen contracts: Linux/Steam Deck and macOS shipping and editors, cross-platform determinism between real devices, crossplay (EOS). Tier 2 (mobile, web, consoles) after |
| v2.0 | Only if a frozen contract truly must break; the upgrade tool migrates |

## Rules for every item

- **Reference samples** (`samples/sandbox` physics party versus, `samples/coop` co-op third person; 4 or fewer samples): an engine item is done only when actually used in at least one reference sample, not by unit tests alone. If a sample gets blocked by a missing engine feature, pull that item forward instead of writing workaround code in the sample. Every sample passes `check.ps1` and Export (from stage 5, on each platform). Samples aren't games: art, content and balance only as much as needed to show features.
- **When finishing**: no Release build warnings, `mp_test.ps1 -Clients 3 -Ticks 900` PASS, the default preset within budget (opt-in features: no GPU cost when off, cost measured when on), no OS/graphics API calls outside the platform layer/RHI, per-tick hashes match in every CI build, the same image on the Vulkan backend, the docs repo updated in English and Korean (`tools/build.py check` passes). New external libraries get `LICENSE` and `mealkit.txt` in `third_party/<lib>/`, then `tools\gen_notices.ps1`.
- **Engine freeze**: before v1.0, an item outside the decided exceptions needs a `docs/DECISIONS.md` row first, and may only add keys or object kinds to frozen formats.

---

## In progress

### [ ] Linux editor finish and editor reinforcement (decided 2026-11-22, `docs/DECISIONS.md` "Linux editor and editor reinforcement come first"; Linux done part in `docs/CHANGELOG.md` "Linux editor")
Order inside the item: A, then B; C and D one piece at a time.
- **A. Linux editor** (moved here from "Editors on other OSes"; macOS stays in stage 5):
  - MkRender on Linux: builds and runs (CHANGELOG "MkRender on Linux"). `--render-golden` on Linux and the editor finding and launching it are done (CHANGELOG "MkRender golden and Open in MkRender on Linux"). Left: hand editing checks, the Manager installing it.
  - Linux paths in the Manager's engine scripts: the project scripts are done (CHANGELOG "Project scripts on Linux"). Packaging scripts: done (CHANGELOG "Packaging scripts on Linux"). Left: the Linux package in the release index (open design question: one index per platform or a `platform` field; `release_index` refuses `linux-x64` names until decided), the Manager on Linux (its `Job.cpp` starts `powershell.exe`, its `install.ps1` is Windows only (COM shortcuts, Visual Studio check), the engine's editor is looked for as `MealKitEditor.exe`; Manager repo), and `vendor.ps1` of MealKit-Contract (its own repo; uses `cmd /c` and Windows `tar.exe`).
  - Linux game exe and Export: done (CHANGELOG "Linux game exe and Export"). `mp_test` between Linux peers passes (CHANGELOG "mp_test on Linux"). Debug, Profile and Presets builds on Linux: done (CHANGELOG "Debug, Profile and Presets builds on Linux"). Left: Steam login on Linux, a clean machine's libraries. Game project editor builds on Linux: done (CHANGELOG "Game project editor on Linux"). Left: an existing game project (FuseTag) is on engine 0.9.5 and needs its engine modules updated before it builds.
  - `check.ps1` on Linux: static, unit and det groups run (CHANGELOG "check.ps1 and CI on Linux"). Left: the tools, render, editor, project and export groups (SKIP on Linux now, except the MkRender capture and the golden images, which run); the first run of the new CI job `linux-frontend` on GitHub.
- **B. Left over from moving scene editing and sprite editing to MkRender** (CHANGELOG "MkRender step 10", "Scene editing in MkRender, leftovers"; MkRender's own list is its `docs/ROADMAP.md`). Left (the rest was checked on Linux 2026-11-22, CHANGELOG "MkRender golden test and editor-kept features on Linux"):
  - Dragging a `gizmo=radius` handle by hand (needs a person at a screen; MkRender's Vulkan build and golden are checked, CHANGELOG "MkRender golden test and editor-kept features on Linux").
  - MkRender's look next to the editor at the same DPI (TOOL_DESIGN section 6 item 7; needs a person looking at both windows), then the MkRender row of section 7.
  - Flipbook clip preview on the selected sprite, once the engine has clips (needs "2D on the 3D engine").
  - The editor's multi-player play start hangs now and then on Linux (about 4 of 20 runs of `--play --players 2|3 --bot`; the next run is fine). One hang under gdb: the main thread sat in `DRM_IOCTL_SYNCOBJ_TIMELINE_WAIT`, a Vulkan wait with an infinite timeout that never returned (candidates in MkRender `src/mkrhi/vulkan/VkDevice.cpp`: `vkWaitForFences` and `vkAcquireNextImageKHR` with `UINT64_MAX`, lines 1139~1141, 1257; Intel MTL, Mesa 26.0.8, 2 or 3 processes drawing at once). Find the cause (or add a timeout that logs and recovers) before the Linux play view counts as done.
  - MkRender coverage and the editor's usability items now live in MkRender's own list (`../MkRender/docs/ROADMAP.md` 4 and 5: scene tree 19~23, inspector 10, viewport 29~31 re-homed there on 2026-11-22; what MkRender doesn't cover yet is listed there too).
- **C. Editor reinforcement** (the editor is now the play view, assets, tables, project and build host): items picked from "Editor usability" below, in its suggested order: 25, 27 (asset browser), 37, 38 (shortcuts, status-bar hints), 39~43 (tables and spreadsheets), build profiles per platform/backend.
- **D. Game code build time** ("Smaller engine items") fits here: opening a project rebuilds the game DLL nearly every time.

### [ ] Templates without game shape: what is left (decided 2026-11-15, `docs/DECISIONS.md` "Templates carry no game shape"; done part in `docs/CHANGELOG.md`)
- Publish `matchflow`, `screens`, `camrig` (GitHub `KingProtea99/module-<name>`, tag `v0.1.0`, entries in MealKit-Modules) so `-Modules screens` works by name; until then pass the folders.
- Registry CI requires headless `tests/`: `screens` and `camrig` are view/app only and have none. Decide: a view-side test build in the shared workflow, or no tests for view-only modules.
- The editor's new project window: a module picker that passes `-Modules` (or leave it to the Manager, which replaces it).
- FuseTag keeps its vendored `sim` 0.1.x with `MatchFlow` until it moves to `matchflow` (`module.ps1 add matchflow`, include and namespace change).

### [ ] 2D on the 3D engine: sprite rendering + editor sprite object (decided 2026-11-10, `docs/DECISIONS.md` "2D games run on the 3D engine")
- **Done** (`docs/CHANGELOG.md` "2D projects and template cards", "Sprites on the 3D engine"): 2D empty template (`templates/empty2d`), `view2d` front view, `PhysicsWorld::setPlane2D`, the sprite pass on DX12 and Vulkan, the `sprite` scene object (sheet cells, pivot, layer, flip, point sampling) drawn in the editor and the 2D template, and sprite editing in MkRender (image picker, sheet slicer, pivot presets, the `view2d` front view; `docs/CHANGELOG.md` "Scene editing in MkRender, leftovers").
- **Left**:
  - Flipbook animation: clips on sprite sheets (frames, frames per second, loop) named in a table so the view picks a clip from game state. Driven by view time, never by `sim/`.
  - Optional pixel snapping with an orthographic camera (`pixelsPerUnit` from the project).
  - A render golden test (`--render-golden`) of a 2D scene loaded by the engine on both backends (the sprite pass itself, its ordering, blending and point sampling, is checked by MkRender's `mkrender_golden` sprites shot).
  - Template sample art: the 2D template draws its characters and platforms as sprites (images in `templates/empty2d/assets/sprites`, CC0 or made by us) instead of boxes, and its preview image is retaken.
- **Rules kept**: view only, nothing enters the simulation (collision stays solid objects, or a solid flag that makes a box of the sprite's size and then enters the content hash). Budget: a few hundred sprites in one or two draws. Lit sprites only as a later opt-in.
- **Later, separate items**: tilemaps (painted grid of sheet cells with merged collision), 2D lights, a parallax layers helper.
- **Checks**: render golden on DX12/Vulkan, an MkRender capture of a sprite scene in its 2D front view, the 2D template built from scratch.

### [ ] UI 2 = MkUI repo (decided 2026-11-11, `docs/ENGINE_MODULES.md` "Middleware")
- **Done**: repo move, draw backend and `ValueModel` (MkUI pinned in `third_party/mkui`), and UI 2a (MkUI Designer as its own app; the editor's [UI] workspace and Penpot import server moved there). Details in `docs/CHANGELOG.md` "UI 2" and "UI 2a".
- **Left**: the XAML-like layer, one piece at a time: data binding (one-way/two-way, converters), styles and control templates, triggers and state animations, resource dictionaries. Own format, no `.xaml` compatibility promise.

---

## Designer independence (decided 2026-11-04, v0.9.x)

Aimed at the moments a designer still needs a programmer. D1~D9 (only D4 is left) are exceptions to "only fixes until v1.0" (`docs/DECISIONS.md` 2026-11-04): they may only add keys and new object kinds to frozen formats.

- [ ] **D4. Squash and spring as data.** Together with FuseTag's character work (M5). `ViewTuning` exposes the numbers, but which event triggers which motion is fixed in FuseTag `view/Juice.cpp`. Move the generic parts into the engine and add feedback columns such as `squash` and `wobble`.
- **Left over from done items**: D1 wasn't checked on a real PC without Visual Studio or on a network share, and the engine and editor binaries are still copied by hand (R2). D2's columns aren't used by a reference sample or a FuseTag row yet. D5 doesn't cover replays, and its Accept button hasn't been clicked in a check.

---

## After publication (proposed 2026-11-05)

The repos are public. What is missing is mostly what a first visitor sees and can try, and what was left of the publication checklist. The first GitHub release is v1.0.0 (`docs/DECISIONS.md` 2026-11-05).

- [ ] Check that the GitHub settings are really on: DCO app, branch protection on `main` (CI must pass), Discussions in `MealKit`, private vulnerability reporting, secret scanning, Dependabot alerts, topics, Pages source Actions for `MealKit-Modules`; the org `.github` repo pushed.
- [ ] Read the Docs for `MealKit-Docs` and the Windows on ARM CI runner (both waited on publication).
- [ ] Sponsorship (the user is looking into it): `FUNDING.yml` in the org `.github` repo once decided. GitHub Sponsors pays out to South Korea (Stripe, W-8BEN); Open Source Collective needs about 100 stars; taxes to check first.
- [ ] A screenshot or play GIF in the engine README (`MealKitEditor --capture`).
- [ ] Full `tools\check.ps1`, then push the remaining local commits.
- [ ] First-run check on a clean Windows PC: build from source, new project, run, export.
- [ ] A short demo video: 4 players and 8,000 physics boxes on integrated graphics.
- [ ] Get outside feedback before freezing contracts for v1.0, since they are hard to take back afterwards.
- [ ] GitHub release v1.0.0 with Windows binaries.

## Waiting on hardware or accounts

Done when the user has the devices or accounts; none of these blocks v1.0 except where stage 5 says so.

- [ ] **Minimum spec hardware** (11th gen Xe): re-measure the performance budget table, shadows and GPU budgets on that PC. Intel↔AMD determinism wasn't run (assumed passed, 2026-10-18).
- [ ] **Real devices for input and online**: a real gamepad (XInput is untested with a pad), real Steam connection tests (assumed passed, 2026-10-18), PIX capture (code only).
- [ ] **EOS backend, the rest** (part 1 done: device ID login, lobby codes, P2P, packet limit 1170 bytes): joining from two PCs and measuring relay RTT/4-player bandwidth (budget 10 KB/s), lobby owner on host migration, Steam login and invites through the portal's Steam provider, an `mp_test` EOS scenario. Needs two PCs and Epic portal setup (two processes on one PC share a device ID). Several online backends enabled together comes with it.

---

## Stage 5 (v1.x): other platforms, editors on other OSes, crossplay

On top of the platform layer and RHI (DX12 + Vulkan), add other OS implementations and the remaining graphics backends. Simulation determinism has been checked by CI on Linux/macOS since stage 1. The goals are **crossplay where every platform joins one match** and **an editor usable on Windows, macOS and Linux**.
Each platform gets a minimum spec/performance budget table when started, and finishes only when all reference samples pass Export and `mp_test` (including peers on other platforms) on that platform.

**Platform tiers**:
- **Tier 1**: Windows, Linux/Steam Deck, macOS. Both editor and shipping. v1.0 waits only for Windows; Linux x64 and macOS ARM64 ship in v1.0 as headless builds, and their windows, renderers and editors come in v1.x (`docs/DECISIONS.md` 2026-11-03).
- **Tier 2** (in order after tier 1): Android/iOS, web, consoles. Shipping only. Consoles start only once developer registration and dev kits are in place.

| Order | Platform | Graphics | CPU/compiler | Check first | Notes |
|---|---|---|---|---|---|
| 1 | Linux · Steam Deck | Vulkan | x64 · Clang (headless exists) | Whether the Windows exe runs as is on Proton (vkd3d-proton). If it does, shipping isn't urgent, but the Linux editor needs native | Steam as is. Case-sensitive file systems |
| 2 | macOS | Metal | ARM64 · Clang (headless exists) | Metal backend, app bundles | Steam as is. Signing/notarization |
| 3 | Android · iOS | Vulkan / Metal | ARM64 · Clang | Mobile GPU budget (tiled GPUs, heat), touch input, ASTC, app lifecycle (what happens to the match when backgrounded?) | No Steam → EOS |
| 4 | Web | WebGPU | wasm · Clang | No UDP (WebRTC data channel transport), threads (SharedArrayBuffer, COOP/COEP), Jolt wasm size | No bindless, wasm 5 MB budget |
| 5 | Xbox | DX12 (GDK) | x64 · MSVC | The closest. GDK's DX12 differences and certification requirements | Common console items in "Consoles" below |
| 6 | PlayStation 5 | proprietary API | x64 · Clang | A proprietary graphics API backend | |
| 7 | Switch (2) | proprietary API (NVN) or Vulkan | ARM64 · Clang | The weakest GPU, so a separate minimum spec table | |

### [ ] Cross-platform determinism between real devices
- **Done on Windows** (2026-11-02): no floating-point contraction in any compiler (`/fp:precise`, `-ffp-contract=off`, no `-ffast-math`, Jolt `CROSS_PLATFORM_DETERMINISTIC=ON`), SimCheck SIM015 (no platform macros in `sim/`), `detmath-bits` matching across CI builds, `mp_test.ps1 -Peer host|join -Remote <address>` (one peer per device, per-tick hash log; two processes on one PC: 900 ticks, `desync=0`). Until passing, the determinism group is in the content hash and peers of other groups are refused.
- **Left**: running it between real devices (Windows ↔ Linux/Steam Deck, Windows ↔ macOS ARM64), recording the results here, then merging the passing groups. Later mobile, wasm and console CPUs/compilers.
- **If it fails**: revisit "Fixed-point gameplay math" in the `docs/DECISIONS.md` deferred table.

### [ ] Crossplay
- **Conditions for the same match**: the determinism group matches; the simulation version matches (game version + hash of `sim/` and engine simulation sources); the content hash is only platform-common data (collision, physics hulls, tuning, tables).
- **Done on Windows** (2026-11-02): content hash from physics hull points only, lobby members carry `net::InputKind`, project setting `mixInputKinds`, player setting `gameplay.crossplay` (`RejectReason::Input`, `RejectReason::Crossplay`). Crossplay matches always go through EOS lobbies/P2P.
- **Left**: verifying a Windows ↔ Linux/macOS match over EOS on real devices. A lobby attribute filter only if a public lobby browser is added.

### [ ] Editors on other OSes (macOS, Linux)
- **Done on Windows** (2026-11-02, v0.8.1): build logic in `tools/build.ps1` (PowerShell 5.1 and 7), `build.ps1` for Linux/macOS, `check_layout` catches asset references with differing case, default symbol visibility and PIC on Linux/macOS Editor builds, `dlopen` loading, game library `<name>_game.dll/.so/.dylib`, editor project paths with `/`.
- **Done on Linux** (2026-11-22): X11 window, input, gamepads, file dialogs, posix processes and images, the editor's build/PowerShell calls, Vulkan Xlib surface; `pwsh build.ps1 Editor` builds and runs the editor (`docs/CHANGELOG.md` "Linux editor").
- **Left**: the Linux items moved to "In progress" ("Linux editor finish and editor reinforcement", 2026-11-22). Still here: macOS window/input/process layers and signed library rules for hot reload. Exporting for platforms the editor's OS can't build (macOS/iOS only on macOS): remote builds or CI, decided then.

### [ ] Consoles (Xbox, PlayStation, Switch)
- **Start conditions**: registration in each platform's developer program and dev kits.
- **Code location**: console SDKs and code on top of them are under NDA and aren't put in this repo. Platform layer implementations, RHI backends and online service backends live in a private repo, and CMake includes them if `MEALKIT_PRIVATE_PLATFORMS` exists (the Steam SDK stub approach).
- **Certification requirements in the design ahead of time**: suspend/resume is a disconnect + reconnect in lockstep matches; user sign-in/switching and controller disconnect notices; saves only through the platform layer's save functions; achievements, friends and invites through the online service layer (crossplay lobbies are EOS).
- **Minimum spec**: Switch gets a separate table.

---

## Smaller engine items

### [ ] Optimization candidates (proposed 2026-10-29) — wait for a measured bottleneck
#1~#4 and #9 are done, #8 (PGO) was measured with no gain and moved to the deferred table. The rest is done only when a measurement shows the cost it removes (`--benchmark` before and after); one that saves nothing is dropped and moved to the deferred table. On `coop --benchmark 15` (2026-11-02) the frame was 2.29 ms, GPU-bound at 2.26 of 8 ms, shadow pass 0.53 of 1.50 ms, so #5 and #7 keep waiting.

| # | Technique | Where | What it saves | Cost / risk |
|---|---|---|---|---|
| 5 | **Static shadow caching** | `Renderer::drawShadows` | Static scene objects drawn into the cascade once and copied each frame while the sun and cascade box don't move; only moving bodies drawn on top | One more depth target per cascade. Gain only with many static objects (terrain, scatter) |
| 6 | **Variable rate shading (tier 1)** | RHI + `Renderer` | Per-draw coarse shading (2×2) for the sky, distant LODs and fog-covered objects (DX12 `RSSetShadingRate`, Vulkan `VK_KHR_fragment_shading_rate`) | Toggleable, off where the device lacks it. Golden images with a tolerance. Last |
| 7 | **Animation LOD** | view-side pose update (`samplePose`, `skinPalette`) | Distant or small characters update their pose every 2nd/4th frame, off-screen ones not at all (the simulation pose path is untouched) | Visible stepping up close; driven by the same screen size as mesh LOD |

None touches `sim/`.

### [ ] External library review follow-ups (reviewed 2026-10-29)
- **Tool repo library rules**: MkAudio, MkUI and Manager have only a README table. Align them with the engine's `mealkit.txt` + generated notices. The full SDL3 source (33 MB) lives in two repos: see whether only what the build needs can be kept.
- **Line endings**: pin `third_party/` with `.gitattributes` in the tool repos so diffs are clean when bumping versions (the ImGui backends differ only in LF/CRLF).
- **New version/CVE check tool**: a script (`tools\check_third_party.ps1`, run by hand since it needs the network) that queries upstream tags and OSV vulnerabilities from each `mealkit.txt` `source`/`version`.
- **Standing rule**: stb_image stays on `STBI_ONLY_PNG/JPEG/HDR` (`ui/StbImpl.cpp`); when adding a format, check that format's CVEs first.

### [ ] Multiplayer modes: P2P, Relay, Dedicated (decided 2026-11-18, `docs/DECISIONS.md` "The game developer picks the multiplayer mode")

Today a match is lockstep with the host as player and relay (`module-net` `Lockstep.h`), over direct UDP, Steam or EOS. The pick and its server address go in `mealkit.project` (view field, not in the content hash) and `ctx.match` follows it; every mode keeps the same protocol.
- **P2P**: today's behaviour, unchanged.
- **Relay**: the same session through `online::Service` relays (Steam Datagram Relay, EOS P2P). Needs the Steam relay check from "Waiting on hardware or accounts" first.
- **Dedicated**: the headless exe as a host with no player slot (today `--host` on the headless exe still takes slot 0): confirms ticks, relays inputs and commands, compares hashes, serves late-join snapshots, keeps the lobby and chat. Clients connect to the address the game gives (`join(address)`). The game developer prepares and runs the server (machine, ports, restarts); the engine only documents how to run it. Open: host migration when the server leaves (end the match, or hand off to a player as today); who may start the match and return to the lobby with no host player; one match per process or several.
- Docs: a manual section in `multiplayer.md` (English and Korean) on choosing a mode, and what Dedicated does not give (below).
- **What no mode gives: hidden information.** Every client runs the whole simulation, so a modified client can read anything in the state: a card game's hands and deck order, or the RNG seed that decides them. Acceptable for co-op and games among friends (deferred table "Server verification"). A competitive card game (RngUnlimited ranked play) would need its own design first, such as server-authoritative state or commit-reveal of hidden cards; record it in DECISIONS before such a mode is built.
- Turn-based games (RngUnlimited): send each decided action (playing a card, ending a turn) as a confirmed command (`sim::sendCommand`) and leave the per-tick input empty. Ticks still flow at the project tick rate (empty 8-byte inputs, a few KB per second per player), which keeps desync checks, replays, rejoin and host migration as they are.

### [ ] Local multiplayer on one PC (shared screen/split screen) — when the first game genre fits
- One peer provides several player slots (tick input stays 8 bytes per player); it can mix with online. Shared camera or split screen (several viewports, `buildFrame` per player), per-player UI focus, input actions assign devices to local players.
- **Determinism**: only the slot count changes. A "two players on one peer" scenario in `mp_test`. **Cost**: medium to large.

### [ ] Game screen UI part 3: Penpot sources for the default screens — when a designer who works in Penpot appears
- Redraw the hand-written default screens (title, menu, join, lobby, loading, pause, results, message, settings, HUD) in Penpot and export them to `art/ui/`. Penpot sources are saved by hand (`docs/DECISIONS.md` 2026-10-20). Check state variants and per-token-set themes in real Penpot at the same time. Screens are otherwise edited in MkUI Designer.

### [ ] Game code build time (proposed 2026-11-12)
Opening a project builds the game DLL when it is missing, older than `mealkit.lib`, older than any engine header or older than its sources (`EditorMain.cpp`, `GameModuleHost::olderThanEngine`/`olderThanSources`), so after an engine rebuild nearly every open builds. FuseTag (20 `.cpp`) spends up to 6 to 13.5 s per file in its `.ninja_log` (`GameSession.cpp` 13.5 s, `GameModule.cpp` 12.4 s) and up to about 14 s of compiling in parallel, plus vcvars64 and CMake configure (not measured yet). Measure the steps first, then:
- Precompiled headers in `mealkit_add_game()` (no target uses `target_precompile_headers` today): the engine headers every `app/` file reparses.
- Cache the vcvars64 environment between builds instead of running `vcvars64.bat` each time.
- Fewer heavy engine headers pulled into game `app/` files.

### [ ] Small leftovers
- Shipping exe size breakdown: FuseTag's Release exe is 4.63 MB of the 5 MB budget (LTCG and `/OPT:REF` already drop unused code). Measure what each library and module takes before the next runtime library is added.
- With a debugger attached, the PDB gets locked and hot reload builds can fail (consider renaming the PDB per build). No editor configuration for the debug CRT (EditorDebug).
- Kernel memory: miniaudio allocation hooks and per-tag budgets.
- `audio` stays in the engine until its glue moves into MkAudio.
- Docs repo: `--dump-reference` reference generation and Weblate.

---

## Modules ⑨: the remaining first modules (one at a time, whenever the user wants one)

The source module system is done (`docs/MODULES.md`); `cooldown`, `objects` and `procgen` exist. One module = one git repo (`KingProtea99/module-<name>`, MIT), registered in `MealKit-Modules`.

| Module | sim | view/UI | Notes |
|---|---|---|---|
| `health` health/damage | hit queries (sphere, capsule, ray), damage (attacker, amount, kind), health, invulnerability ticks, death → remove Pawn → respawn wait → possess a new Pawn | health bar and damage number `.ui` | Uses the game flow framework. Hit results are processed in id order |
| `inventory` inventory | fixed-array slots, item definitions in a `.table` | grid `.ui` | Connected to saves and match start data |
| `minimap` minimap | none | top-down render or icons | View only |
| `ai` decision making | utility scoring + `enum` state machines per `Controller`, perception (sight cones, hearing) via physics queries | debug drawing | Uses "AI pathfinding" for movement. No behavior tree graph editor (DECISIONS 2026-10-29) |

---

## Tool suite (runs separately from the stages)

MealKit is a suite with the engine + general-purpose editor at the center, where small tools for workflows that differ per role are built as **separate repos** and tied together by editor shortcuts (`docs/DECISIONS.md` 2026-10-26). All free, code is MIT. Jobs that already have good free external tools stay external tool + engine plugin. Tools are split off one at a time; when to start is the user's call. Done so far: the contract between tools (MealKit-Contract), MkAudio (runtime + authoring tool), MkQA, MkUI (runtime + Designer).

| Tool (working name) | Role | Now | Direction |
|---|---|---|---|
| Level editing | Level designers | general-purpose editor scene editing | Deeply tied to scenes, Reflect and the content hash, so it shares the engine core. Boundaries are decided when splitting |
| Data management | Designers | editor [Data] window | For the same reason, boundaries are decided when splitting |
| Code work | Programmers | VS Code, Visual Studio | An own IDE is deferred. External IDE integration first |

- [ ] **MkRender leftovers** (`docs/DECISIONS.md` 2026-11-12; `mkrhi`, `mkrender` and its render goldens without the engine are done, CHANGELOG "MkRender leftovers: render goldens without the engine"): whether to publish it as a product of its own.
- [ ] **MkQA next candidates**: running `tests/*.scenario` with results, turning a report's recording into a scenario, `mp_test` runs.
- [ ] **UI 3 leftovers**: the old preview-tool proposal is covered by MkUI Designer; check whether it still lacks a gamepad focus walk and `--ui-check` box differences, and add them to MkUI if so.
- [ ] **UI features after v1.0** (one at a time as games need them, each checked against the 11th gen Xe budget): keyframe timeline animation (deferred table), virtualized long lists, right-to-left and complex-script shaping, per-player focus for local multiplayer, UI cost numbers (quads, layout ms) in the benchmark budget.
- [ ] **Shipped sound size and streaming** (MkAudio + engine cook, the FMOD bank equivalent without a bank format; 2026-11-15). Today cook copies `.wav`/`.ogg` into the pack as they are (`game/Cook.cpp` `binary()`), and a clip read through `readFile` from a pack is loaded whole, so `stream` music isn't streamed in Export builds (`mkaudio/Mixer.h`). FuseTag ships 2.3 MB of wav, three music stems about 640 KB each.
  - Cook encodes `.wav` clips to Ogg Vorbis (the runtime already decodes it with stb_vorbis; the encoder runs only at cook time, so it isn't in the shipping exe budget). Choose the encoder library and its quality setting with the user first (`docs/DECISIONS.md` row). A per-event or per-folder opt-out keeps short sound effects as wav where decode cost or latency matters; `sounds.events` paths stay the source names.
  - Streaming from the pack: a ranged read (offset + size inside the pack) in place of the whole-file `readFile` for `stream` events and music stems, so music is read in pieces in Export builds as it is from disk in dev builds.
  - Per-scene clip sets with a memory budget stay in "Audio features after v1.0" below.
- [ ] **Audio features after v1.0** (in MkAudio, runtime + tool together, one at a time): bus effect chains (EQ, compressor/limiter, delay) and sidechain compression in place of volume-only ducking, master limiter + loudness meter (LUFS), clip sets loaded/unloaded per scene with a memory budget warning, a voice/profiler timeline over the live `audio/` state, dialogue lines (queue, priority, tied to subtitles), Steam Audio HRTF (deferred table).
- [ ] **Achievements, leaderboards, stats after v1.0** (`docs/DECISIONS.md` 2026-11-02): reviewed again once the platform SDK work (Steam, EOS, console trophies) is confirmed. Plan as it stood: the game defines achievement conditions as simulation events, the view side records them through the online service layer, the achievement list is one piece of data exported for each service, and without a service stats are kept in the user data folder. No effect on determinism.
- [ ] **Opt-in rendering features after v1.0** (`docs/DECISIONS.md` 2026-11-04, one at a time, each with its cost measured on minimum spec hardware and shown to the developer; view-side; additions only). Shared first piece: project settings for each feature, player quality presets (Low, Medium, High) with a per-game lock, and an editor warning when the enabled features exceed the project GPU budgets. Order: (1) project surface shaders (API and pipeline variant handling designed first), glTF occlusion texture applied to ambient light only, point light shadows on chosen lights (shadow atlas), UI blur, screen-space outlines, mesh particles → (2) post passes that don't run when off: SMAA, EASU upscaling, depth of field, motion blur, SSR → (3) RHI compute on DX12 and Vulkan, then Forward+, SSAO and GPU particles → (4) real-time GI, technique chosen when started. Deferred rendering only when a game shows a need Forward+ can't meet. What a technical artist needs around surface shaders is T6~T9 and T11 below.
- [ ] **Design refresh for the rest of the suite** (planned 2026-11-05, approach settled by `docs/DECISIONS.md` 2026-11-12: each tool copies `applyStyle`, no shared style library). State per app is `docs/TOOL_DESIGN.md` section 7; each tool is done when the checklist in section 6 holds. App icons and the theme, icon font, icon buttons and title bar of every tool are done (CHANGELOG). Left:
  - **Tools**: a right-click item for each icon action (TOOL_DESIGN section 1; MkRender done), QA's one window on the gutter and panel layout (section 6 item 3).
  - **Editor**: tree and table arrows, still ImGui's triangles.
  - **Tokens and text (2026-11-13, `docs/DECISIONS.md` "Suite look follows public design systems")**: the TOOL_DESIGN 3.1 colors, ramp and spacing in MkAudio, MkUI, MkRender and QA (Tokens column of section 7); QA and each tool's first screen to icon + label buttons with one primary button (MkRender done).
  - **Manager screens**: a left navigation instead of the top tabs; a project details pane (module summary with `Update all`, engine and folder under a collapsed "Details"), so the Modules tab is not the only place to see updates.

---

## Editor usability (proposed 2026-11-04 and 2026-11-09, not scheduled)

Every ★ item of the 2026-11-04 review is done; what is left isn't decided. Pick items before work starts, and put open design questions in `docs/DECISIONS.md` first. Editor-only, doesn't change the shipping exe, and adds no authoring tools inside the editor (criterion ④). Game UI editing moved to MkUI Designer, so its items are MkUI's. Numbers are those of the 2026-11-04 list.

**Suggested order**: 25, 27 (asset browser, as a project's assets grow) → 33 → the rest when someone asks. The scene tree (19~23), inspector (10) and viewport (29~31) items moved to MkRender on 2026-11-22 (its `docs/ROADMAP.md` 4): the editor keeps only a read-only preview, so they are not editor work.

- **Inspector** (scene object inspector: MkRender's, `../MkRender/docs/ROADMAP.md` 4): item 10 and its left-over parts (tabs in field groups 2, asset/event picker drag and drop and preview 4, `validate` rules 6) moved there.
- **Scene tree** (MkRender's hierarchy, `../MkRender/docs/ROADMAP.md` 4): items 19~23, separators (18) and the prefab badge (20) moved there.
- **Asset browser**: 25. search, kind filters, favorites, a folder path bar and recently used assets. 27. preview pane: triangle count, budget bar, sound waveform and a play button.
- **Visual tools**: 33. event and desync markers on the play timeline (the editor's play view). 30 (viewport icons and labels per kind), 31 (small preview of the selected virtual camera) and `gizmo=point` (29) moved to MkRender's view (`../MkRender/docs/ROADMAP.md` 4).
- **Shared UI and workflow**: 37. rebindable shortcuts. 38. context hints in the status bar for the current tool and hovered item. Left from done items: icons with tooltips on the viewport toolbar (35).
- **Tables and spreadsheets (proposed 2026-11-18)**: `.table` stays the source (text: git diff and merge, RowRef checks, content hash); Excel or Google Sheets is only for editing, as Unreal teams do with DataTable + CSV reimport, but without a binary asset. In `module-data` (`TablesPanel.cpp`, `Tables.cpp`), in this order: 39. copy and paste of cell ranges as tab-separated text, both ways with Excel, starting at the selected cell. 40. a change preview before CSV import (changed cells, new rows, removed rows), since import replaces the whole table. 41. remember the last CSV path per table and a **Reimport** button through that preview; watching the file for changes later. 42. min/max ranges on fields, marked in the panel and warned on load. 43. bulk edit of one column over selected rows (a value, or a percentage), low priority once 39 exists. Known Excel traps the import checks should cover: CSV is saved as cells are formatted (`0.125` shown with 2 decimals is written as `0.13`), automatic conversion of ids and values (`1-2` to a date, `1.10` to `1.1`, leading zeros), plain "CSV" saved as CP949 on Korean Windows instead of UTF-8, and the file locked while Excel has it open. No formulas in the engine: they stay in a spreadsheet.
- **From the 2026-11-09 feedback review**: build profiles per platform/backend (like Unity Build Profiles, Godot Export Presets): profiles for Windows DX12 / Windows Vulkan / demo / debug with an output folder each, OSes added with stage 5; decide first whether profiles live in `mealkit.project` lines or files. Own move/scale gizmo drawing (round ends, outline, hover growth) while rotation and the axis cube stay ImGuizmo, so `third_party/imguizmo` isn't patched.

## Visual feedback proposals (drafted 2026-10-29, not scheduled)

Goal: **a designer adds light and glow to an event with one `feedback.table` row**, like shake. Same rules as the existing columns: strength from `strengthFrom`/`strengthFull`, position from `positionFrom`, view side only, scaled by player settings, previewed in the [Feedback] panel. Event lights, glow kicks and hit flash are done (designer independence D2, `docs/CHANGELOG.md` "Feedback table: lights, kicks, hit flash"); nothing below is decided. The open questions become `docs/DECISIONS.md` rows when an item is picked.

**More feedback columns**
- [ ] **Object glow pulse** (`emissivePulse`, `emissivePulseSeconds`, target from `targetFrom`): the hit or activated object's emissive and rim light up and fade. Open: where the per-instance emissive scale lives (share the flash byte or add a word to the 64-byte instance).
- [ ] **Shockwave distortion** (`shockwave`, `shockwaveRadius`, `shockwaveSeconds`): a ring that bends the picture outward; needs the distortion pass (D below) and follows the shake setting.
- [ ] **Accessibility**: one "flashing lights" setting multiplying `light`, `hitFlash`, `bloomKick`, `exposureKick` and `flash` (open: one slider or two), and a photosensitivity guard limiting large whole-screen brightness changes to 3 per second.
- [ ] **Editor**: View Mode > Event Lights (each active event light as a wire sphere with its range); the preview fires `hitFlash`/`emissivePulse` at the look dev object. Open: hit flash colors other than white (instance data size).

**Shading techniques** (each a toggle; cost is a guess for the minimum spec, measured with `--benchmark` before deciding; ordered by effect per millisecond; overlaps with "Opt-in rendering features" are built there)

| # | Technique | What it gives | Rough cost | Notes |
|---|---|---|---|---|
| A | Decals (projected boxes) | Scorch marks, footprints, blob shadows; events leave traces | 0.2–0.5 ms | Also a `decal` feedback column. Box projection in the forward pass via a per-instance decal list like point lights. Open: feedback-only or also placed scene objects |
| B | SSAO (half-res GTAO/SAO) | Objects sit on the ground | 0.6–1.0 ms | Needs scene depth; Toon can use it as a band darkener |
| C | Soft particles | Particles stop cutting lines into surfaces | ~0.1 ms | Depth fade in the particle shader (check what `.fx` `softness` does) |
| D | Screen distortion pass | Heat haze, shockwaves, water refraction | 0.2 ms | A small distortion target sampled in the uber pass |
| E | Height fog + sun light shafts | Atmosphere, depth | 0.3–0.5 ms | Light shafts as a quarter-res radial blur |
| F | Animated emissive (glTF `extras`) | Glowing runes, screens, lava without keys | ~0 | Material feature like rim; Blender add-on field |
| G | Spot lights + cookies | Flashlights, stage lights | like point lights | Same per-instance light table plus a cone; no shadows |
| H | Edge-detect outline (post) | Cleaner Toon outlines with inner lines | 0.3 ms | On top of the hull outline |
| I | Screen space contact shadows | Small sharp shadows near the camera | 0.3 ms | Short depth ray march toward the sun |
| J | Subsurface wrap | Softer skin and wax | ~0 | Wrap term + tint per material |
| K | Hatching | Drawn look for Toon shadows | ~0 | Driven by the toon ramp value |

Suggested order: A and C → B and F → D, then the shockwave column → the rest when a game needs them. Not for the minimum spec now: SSR, TAA, froxel volumetric fog, ray tracing, deferred shading.

## Technical artist proposals (proposed 2026-11-04, not scheduled)

Aimed at a technical artist (TA) who owns a game's look: writing shaders, checking their cost and keeping the look from breaking. **Nothing here is decided yet**: pick items before work starts, and put open design questions in `docs/DECISIONS.md` first. Sizes are estimates. ★ marks the highest-value items.

**What a TA has today**: style presets, Blender material features, [Look Dev], view modes, GPU capture, `--benchmark` and post values with LUT grading. A TA can't write a shader for a game: `render::ShaderReloader` watches only MkRender's `shaders/`, and "project surface shaders" is the first item of "Opt-in rendering features".

**Principle**: shaders are text edited in an external editor, and looks are checked and tuned in the MealKit editor. A node graph is reviewed only if surface functions have been used and TAs who don't write code are blocked by them.

**Engine freeze**: T1~T5 and T10 are editor or tool work or debug-only shader paths, so they fit an exception well, but each still needs a row before v1.0. T6~T9 and T11 build on surface shaders, which come after v1.0. All are view-side.

**Suggested order**: T1, T2, T4 (small, useful with the current presets) → T5, T3 when a TA is comparing looks (T10 if an engine upgrade breaks a game's look) → T6, T7, T8 in the surface shader design, then T9, T11.

### Checking a look (no project shaders needed)
- [ ] T1. ★ **More view modes** (S). Add base color, roughness, metallic, emissive, sun only, ambient only, reflections only, shadow cascades, mip level and texel density to `render::DebugView`. They extend the `debugView` branches in `PSMain` (`Forward.hlsl`), which are compiled into the shipping shader today, so the new modes sit behind a define that builds only with the debug UI. DX12 and Vulkan.
- [ ] T2. ★ **Pixel inspector** (S). Click the scene view or [Look Dev] to see that pixel's linear HDR value, luminance, value after exposure and final sRGB. Needs a readback of the HDR target (`Renderer::captureTarget` reads 8-bit targets today).
- [ ] T3. **[Look Dev] references and comparison** (M). A grey ball, a chrome ball and a color chart beside the model; a split view of two states (lighting model, HDRI, LUT or a saved image).
- [ ] T4. ★ **[Materials] page in [Look Dev]** (S). Read-only list of the selected model's materials: factors, textures with size and format, features and their values, the features ignored on load, and which meshes use each one; clicking a row highlights those triangles. Values stay in Blender.
- [ ] T5. ★ **Cost comparison** (M). A page like [Debug] > [Balance] that runs `--benchmark` on a scene twice (before and after a change, or with a project setting flipped) and shows the difference per `render::GpuPass`, saying which GPU the numbers came from.

### Writing project shaders (with "project surface shaders")
- [ ] T6. ★ **Project shader files and hot reload** (M). `assets/shaders/*.surface.hlsl`, chosen per material from the Blender MealKit panel (a glTF `extras` key in the engine schema). `ShaderReloader` also watches the project folder, and the cook compiles them, so the shipping exe needs no compiler. Used by water and lava in a reference sample before it counts as done.
- [ ] T7. ★ **Parameters declared in the shader** (M). A block at the top of the file (name, type, range, default, texture) read at compile time and shown as Blender panel fields and [Look Dev] sliders; saving goes to the glTF source through Blender.
- [ ] T8. ★ **Compile errors where the TA looks** (S). Failed reloads go to [Debug] > [Problems] and a notification, opening `file:line` in the code editor, with lines mapped back to the surface file. A broken shader keeps the previous one.

### Keeping a look under control
- [ ] T9. ★ **Shader budgets** (M). Project limits on texture samples and estimated instructions per surface shader from DXC's output, reported as warnings in [Debug] > [Problems] and the Blender add-on. A list shows every project shader with its pipeline count, numbers and users.
- [ ] T10. **Project look tests** (M). `--render-golden` for a game project's own shots (`tests/golden/`, `shots.txt`, an update button in [Debug] > [Tests]) on both backends. Works with style presets alone.
- [ ] T11. **Shared library and external editor setup** (S). An engine include with noise, UV and color helpers as part of the surface API, generated VS Code HLSL settings (include paths, defines), and a cookbook page with water and lava.

### Later, when a game asks
- T12. Project post effects: one full-screen function in the same form as surface functions, an opt-in pass with no cost when off.
- T13. Shaders in source modules, so a look can be shared between projects. Needs the surface API frozen first.
- T14. Project shaders for particles (`.fx`) and `.ui` nodes.

### Open design questions
- Surface API shape: one `Surface surf(Input)`, or also a vertex function for waves and wind? The vertex function is hard to take back, as is every public `Input` field.
- TAs without a build toolchain: hot reload and cooking compile with DXC, which arrives with the first engine build, so a PC on the D1 shared folder may have neither. Does the editor bundle or download DXC, and where do the engine includes come from?
- Pipeline count: each surface shader multiplied by lighting model, skinned, shadow and outline pipelines, all built on load. How many are acceptable, and what does loading cost?
- Parameter storage: the material record is a fixed 128 bytes. Do custom parameters get a separate table in the upload ring?
- Later backends: surface files must go through the same stage 5 shader conversion as engine shaders.

## Roles and tools review (proposed 2026-11-04, not scheduled)

A pass over every game development role, applying criterion ④ role by role. **Nothing here is decided yet**. It comes from documents and code searches, not from running each feature, so check an item's "now" before building on it.

**Conclusion**: most roles work in external tools and need only a connection from us. The real gaps are in the work roles share: collaboration setup, installing the tools, and capture for trailers.

**How to sort a job**: judged away from our runtime → external tool (modeling, composing, scheduling); judged only inside the real game → external, and we build import, preview and checks (textures, materials, animation); tied to our data formats or determinism rules → the editor or a MealKit tool (tuning, scenes, rule data, replays).

| Role | Now | What we'd add |
|---|---|---|
| Systems and balance design | [Data], [Balance], [Tests] | D9 |
| Level design | Scenes, terrain brushes, prefabs | D9 triggers, platforms, timers |
| Narrative | No dialogue system | R4, when a game needs it |
| Texture art | Texture cache, [Look Dev] | T4 |
| Animation | State table, animation events | IK and root motion when a game asks (deferred table) |
| Technical art | Proposals only | T1, T2, T4, T6~T8 |
| VFX | Own particle runtime | Mesh and GPU particles as opt-ins |
| UI/UX | MkUI Designer; Penpot optional | Nothing (Figma when designers can only use Figma) |
| Sound and music | DAW → MkAudio | "Audio features after v1.0" |
| Localization | Per-language string files, font glyph check | R5 |
| Build and release | D1 shared folder | R6; D1's check on a real PC |
| Data analysis | No telemetry | R7 |
| Marketing | No capture mode | R3 |

Gameplay, engine and network programming, concept art, modeling and rigging, cinematics (no sequencer, decided), QA and production need nothing from us.

- [ ] R1. ★ **Collaboration setup** (S). `templates/project` has `.gitignore` but no `.gitattributes`, and nothing documents Git LFS or scene merge conflicts. Add a `.gitattributes` to the template (bumps `VERSION`) and a manual page in English and Korean. Best done soon, since new projects copy the template. Open: is Git LFS the default or a recommendation (hosting quotas)? Do `.scene` files merge cleanly as text?
- [ ] R2. ★ **Installing without a build** (M; `docs/DECISIONS.md` "Users download only MealKit Manager" and "How the Manager installs"). Done: engine, Manager and tool packages (`tools\package_engine.ps1`, `package_tool.ps1`, the Manager's `scripts\package.ps1`), the index writer (`tools\release_index.ps1`), and in the Manager engine and tool installs, engine matching, the compiler check, shortcuts, update and uninstall (CHANGELOG "R2 ..." parts 1 to 7). Left:
  - Publishing: upload the engine, Manager and tool zips to their repos' GitHub Releases and publish `index.json`, then make that address the Manager's default (today only `MEALKIT_RELEASE_INDEX`). Which repo holds the index (a new `MealKit-Releases` or `MealKit-Modules`) is picked then.
- [ ] R3. **Free camera in replays** (M). Input replays and a spectator camera mode (`Free` in `scene/Scene.h`) exist, so this may be mostly connecting them, plus hiding the UI and stepping time. For trailers, screenshots and QA. Needs a DECISIONS row before v1.0. Open: shipping exe, editor or both; photo mode for players or dev-only?
- [ ] R4. **Where dialogue state lives** (decision first, S): simulation or view state decides how it syncs in multiplayer. Record it in `docs/DECISIONS.md` before any module is written.
- Later, when a game asks: R5 translation exchange (string files to and from a translation tool's format, missing keys, overflowing text); R6 store upload script around `steamcmd` after Export; R7 analytics module (needs a server, stays outside the engine).
- **Not to build**: scheduling, issue tracking and document tools; modeling, texture painting and composing tools; visual scripting and a sequencer.

## AI agents and game developer ease (proposed 2026-11-18, not scheduled)

A review of how well an AI agent (and a developer new to MealKit) can read the engine and make a game with it. **Nothing here is decided yet**; pick items before work starts.

**Already strong**: one answer per problem (`docs/CODING_MODEL.md`, the template CLAUDE.md and the cookbook teach the same patterns); mistakes caught by tools at build time with what to use instead (SimCheck `SIMnnn`, STR001, `check_layout`); every piece of game data is text (`.table`, `.tuning`, `.ui`, `.events`), so an agent edits data as well as code; self-checks without a person (`--bot`, `--capture`, `tests/*.scenario`, `mp_test`, replays); one `MEALKIT_FIELD` declaration reaches the hash, snapshots and editor panels.

- [ ] A1. ★ **Build traps fixed in the build, not around it** (M). Today a UTF-8 console makes ninja lose header dependencies, and stale objects after a class layout change crash at run time. The workspace avoids both only with agent-side hooks (the build wrapper script, `guard.py`, the `crash-triage` skill), so a developer using another agent or their own shell still hits them. Fix it inside `tools\build.ps1` (code page set by the build itself, dependency check after the build) so every caller is safe. Open: can the wrapper's dependency check move into the build as it is?
- [ ] A2. ★ **Vendored copies marked as copies** (S). An engine module's files exist in its repo `module-<name>`, in the engine's `modules/<name>` and in each game's `modules/<name>` (`Tables.cpp` has four copies today), so an agent easily edits the wrong one. A one-line header at the top of each copied file ("vendored copy of module-<name> vX; change it in that repo"), added by `module.ps1 add/update` and `engine bump`, and a `check_layout` warning when a copy differs from `module.files` without being recorded as a local change. Open: does the header change the files `update` compares (`module.files` hashes)?
- [ ] A3. **API index for agents** (M, together with "Docs repo: `--dump-reference`" in "Small leftovers"). One short text file per module and per kernel folder: each public type and function on one line with its header and a few words. Generated, never written by hand, shipped with the engine and with each module copy. Today an agent greps headers, and LSP needs an Editor build first.
- [ ] A4. **Shorter project agent notes** (S, bumps `VERSION`). Split the template `templates/project/CLAUDE.md` (59 dense lines) into about ten core rules plus an index "when doing X, read Y" (cookbook pages, module `docs/index.md`, CODING_MODEL rows). Add an `AGENTS.md` with the same content (or pointing to it) for agents other than Claude Code.
- [ ] A5. **More cookbook recipes** (S each). Jobs a game repeats: add a weapon (table row + RowRef + feedback row), add an enemy kind, add a game mode, add a map gimmick with `objects`. Each one is also the pattern an agent copies. English and Korean together.
- [ ] A6. **Asking the running game and editor** (M; DECISIONS row first). Today an agent sees only screenshots and logs. A read-only query that answers in JSON: `WorldState` fields at a tick, table rows, missing RowRefs, load warnings, the content hash parts. First as a command line option on the headless exe (`--query`), then maybe an MCP server inside the editor. Open: command line only or a live server; what the editor may let an agent change (play, step, select) and what stays read-only.
- [ ] A7. **Load warnings that say what to do** (S, ongoing). Like SimCheck: each warning when a table, tuning file, scene or UI file is read names the file and line and the fix ("unknown field `speeed` in tables/props.table:12; did you mean `speed`?").
- Higher-level building blocks (Modules ⑨: `health`, `inventory`, `minimap`, `ai`) help game developers and agents the most, and stay in that section.

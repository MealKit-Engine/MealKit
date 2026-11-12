# MealKit engine roadmap

**Only work that is left.** A finished item is removed from this document and its details go to `docs/CHANGELOG.md`; a name quoted elsewhere as ROADMAP "…" for a finished item is found there by the same name. Reasons for decisions and the "Deferred" table are in `docs/DECISIONS.md`. When the order changes, update this document.

## Where it stands (v0.9.4, 2026-11-12)

Stages 0~4 are done (v0.4~v0.8), the contract freeze review and the cookbook are done (v0.9.0, `docs/CONTRACTS.md`), the engine is a kernel plus 20 engine module repos (`docs/ENGINE_MODULES.md`), the game UI runtime and its Designer live in MkUI (UI 2a), and the repos are public (first green public CI on Linux x64 and macOS ARM64 2026-11-03). Until v1.0 only fixes go in, plus the decided exceptions that only add to frozen formats (designer independence, the game UI editor; `docs/DECISIONS.md` 2026-11-03 and 2026-11-04).

**Next, in order**:
1. 2D on the 3D engine: what is left of the sprite item.
2. Designer independence D9 (before FuseTag M6) → D4 (with FuseTag M5) → D8.
3. UI 2: MkUI's XAML-like layer, one piece at a time.
4. After publication: the checks and the v1.0.0 release.
5. When devices and accounts are available: EOS measurements, cross-platform determinism and crossplay on real devices, minimum spec measurements → stage 5 (v1.x).

Not scheduled (picked before work starts): the open editor usability items, visual feedback proposals, technical artist T1~T14, roles review R1~R7, optimization #5~#7, modules ⑨, the tool suite rows.

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
| v0.9 (now 0.9.4) | v1.0 candidate: contracts reviewed, cookbook written; only fixes and the decided exceptions until v1.0 |
| **v1.0** | **Windows** (editor and shipping, DX12 and Vulkan) is fully supported and **Linux x64 / macOS ARM64 run the headless simulation** (per-tick hashes match Windows in CI on every commit). All reference samples pass Export and the determinism checks on Windows, and **contracts are frozen** (`docs/CONTRACTS.md`) |
| v1.x | Stage 5 tier 1 without breaking the frozen contracts: Linux/Steam Deck and macOS shipping and editors, cross-platform determinism between real devices, crossplay (EOS). Tier 2 (mobile, web, consoles) after |
| v2.0 | Only if a frozen contract truly must break; the upgrade tool migrates |

## Rules for every item

- **Reference samples** (`samples/sandbox` physics party versus, `samples/coop` co-op third person; 4 or fewer samples): an engine item is done only when actually used in at least one reference sample, not by unit tests alone. If a sample gets blocked by a missing engine feature, pull that item forward instead of writing workaround code in the sample. Every sample passes `check.ps1` and Export (from stage 5, on each platform). Samples aren't games: art, content and balance only as much as needed to show features.
- **When finishing**: no Release build warnings, `mp_test.ps1 -Clients 3 -Ticks 900` PASS, the default preset within budget (opt-in features: no GPU cost when off, cost measured when on), no OS/graphics API calls outside the platform layer/RHI, per-tick hashes match in every CI build, the same image on the Vulkan backend, the docs repo updated in English and Korean (`tools/build.py check` passes). New external libraries get `LICENSE` and `mealkit.txt` in `third_party/<lib>/`, then `tools\gen_notices.ps1`.
- **Engine freeze**: before v1.0, an item outside the decided exceptions needs a `docs/DECISIONS.md` row first, and may only add keys or object kinds to frozen formats.

---

## In progress

### [ ] 2D on the 3D engine: sprite rendering + editor sprite object (decided 2026-11-10, `docs/DECISIONS.md` "2D games run on the 3D engine")
- **Done** (`docs/CHANGELOG.md` "2D projects and template cards", "Sprites on the 3D engine"): 2D empty template (`templates/empty2d`), `view2d` front view, `PhysicsWorld::setPlane2D`, the sprite pass on DX12 and Vulkan, the `sprite` scene object (sheet cells, pivot, layer, flip, point sampling) drawn in the editor and the 2D template.
- **Left**:
  - Editor: an image picker and sheet slicer (columns/rows, preview of the chosen cell) in the inspector, pivot presets; picking and the gizmo on the quad (rotation about z only in `view2d`); dropping a PNG from the asset browser onto the viewport makes a sprite; thumbnails in the asset browser.
  - Flipbook animation: clips on sprite sheets (frames, frames per second, loop) named in a table so the view picks a clip from game state. Driven by view time, never by `sim/`.
  - Optional pixel snapping with an orthographic camera (`pixelsPerUnit` from the project).
  - A render golden test (`--render-golden`) for ordering, blending and point sampling on both backends.
  - Template sample art: the 2D template draws its characters and platforms as sprites (images in `templates/empty2d/assets/sprites`, CC0 or made by us) instead of boxes, and its preview image is retaken.
- **Rules kept**: view only, nothing enters the simulation (collision stays solid objects, or a solid flag that makes a box of the sprite's size and then enters the content hash). Budget: a few hundred sprites in one or two draws. Lit sprites only as a later opt-in.
- **Later, separate items**: tilemaps (painted grid of sheet cells with merged collision), 2D lights, a parallax layers helper.
- **Checks**: render golden on DX12/Vulkan, an editor capture of a sprite scene in `view2d`, the 2D template built from scratch.

### [ ] UI 2 = MkUI repo (decided 2026-11-11, `docs/ENGINE_MODULES.md` "Middleware")
- **Done**: repo move, draw backend and `ValueModel` (MkUI pinned in `third_party/mkui`), and UI 2a (MkUI Designer as its own app; the editor's [UI] workspace and Penpot import server moved there). Details in `docs/CHANGELOG.md` "UI 2" and "UI 2a".
- **Left**: the XAML-like layer, one piece at a time: data binding (one-way/two-way, converters), styles and control templates, triggers and state animations, resource dictionaries. Own format, no `.xaml` compatibility promise.

---

## Designer independence (decided 2026-11-04, v0.9.x)

Aimed at the moments a designer still needs a programmer. D1~D9 are exceptions to "only fixes until v1.0" (`docs/DECISIONS.md` 2026-11-04): they may only add keys and new object kinds to frozen formats.

- [ ] **D9. Generic objects, then condition → effect rows.** Next, before FuseTag M6, so its three map gimmicks are built and checked with it. New item effects are fixed in code today (FuseTag `ItemEffect` and the `switch` in `sim/Items.cpp`), and map gimmicks need new object kinds and `sim/` code.
  1. The `objects` module exists (trigger volumes, platforms on paths; 2026-11-09). Add a timer object.
  2. Rule rows on trigger objects or tables, such as `player enters → speed ×0.5, 3 s`. Programmers define the list of effects and designers combine them. This keeps "no visual scripting". Determinism holds as long as the data is in the content hash and the rows run at a fixed point in `sim/`.
  3. Scripting languages stay deferred.
  - **Decide before starting** (`docs/DECISIONS.md`): rule rows on trigger objects or in a table, the first list of effects, and whether FuseTag's gimmicks written as their own scene objects (M6 `Blackout`) move to rows.
- [ ] **D4. Squash and spring as data.** Together with FuseTag's character work (M5). `ViewTuning` exposes the numbers, but which event triggers which motion is fixed in FuseTag `view/Juice.cpp`. Move the generic parts into the engine and add feedback columns such as `squash` and `wobble`.
- [ ] **D8. Tuning A/B presets** (editor-only, any time): switch saved value sets during play and show differences from the file. [Debug] > [Balance] already compares two tuning files across bot matches, but not during play.
- **Left over from done items**: D1 wasn't checked on a real PC without Visual Studio or on a network share, and the engine and editor binaries are still copied by hand (R2). D2's columns aren't used by a reference sample or a FuseTag row yet. D5 doesn't cover replays, and its Accept button hasn't been clicked in a check.

---

## After publication (proposed 2026-11-05)

The repos are public. What is missing is mostly what a first visitor sees and can try, and what was left of the publication checklist. The first GitHub release is v1.0.0 (`docs/DECISIONS.md` 2026-11-05).

- [ ] Check that the GitHub settings are really on: DCO app, branch protection on `main` (CI must pass), Discussions in `MealKit`, private vulnerability reporting, secret scanning, Dependabot alerts, topics, Pages source Actions for `MealKit-Modules`; the org `.github` repo pushed.
- [ ] Read the Docs for `MealKit-docs` and the Windows on ARM CI runner (both waited on publication).
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
- **Left**: Linux/macOS window, input and process layers; the editor's [Build] menu calling `pwsh build.ps1` there (with posix `Process`); building the editor on those systems; on macOS, signed library rules for hot reload. Exporting for platforms the editor's OS can't build (macOS/iOS only on macOS): remote builds or CI, decided then.

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

### [ ] Local multiplayer on one PC (shared screen/split screen) — when the first game genre fits
- One peer provides several player slots (tick input stays 8 bytes per player); it can mix with online. Shared camera or split screen (several viewports, `buildFrame` per player), per-player UI focus, input actions assign devices to local players.
- **Determinism**: only the slot count changes. A "two players on one peer" scenario in `mp_test`. **Cost**: medium to large.

### [ ] Game screen UI part 3: Penpot sources for the default screens — when a designer who works in Penpot appears
- Redraw the hand-written default screens (title, menu, join, lobby, loading, pause, results, message, settings, HUD) in Penpot and export them to `art/ui/`. Penpot sources are saved by hand (`docs/DECISIONS.md` 2026-10-20). Check state variants and per-token-set themes in real Penpot at the same time. Screens are otherwise edited in MkUI Designer.

### [ ] Small leftovers
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

MealKit is a suite with the engine + general-purpose editor at the center, where small tools for workflows that differ per role are built as **separate repos** and tied together by editor shortcuts (`docs/DECISIONS.md` 2026-10-26). All free, code is MIT. Jobs that already have good free external tools stay external tool + engine plugin. Tools are split off one at a time; when to start is the user's call. Done so far: the contract between tools (MealKit-Contract), MkAudio (runtime + authoring tool), MealKit-QA, MkUI (runtime + Designer).

| Tool (working name) | Role | Now | Direction |
|---|---|---|---|
| Level editing | Level designers | general-purpose editor scene editing | Deeply tied to scenes, Reflect and the content hash, so it shares the engine core. Boundaries are decided when splitting |
| Data management | Designers | editor [Data] window | For the same reason, boundaries are decided when splitting |
| Code work | Programmers | VS Code, Visual Studio | An own IDE is deferred. External IDE integration first |

- [ ] **MkRender: `mkrender`** (`docs/DECISIONS.md` 2026-11-12, `docs/ENGINE_MODULES.md` "Middleware"; `mkrhi` is done): the renderer in the repo MkRender with its own plain inputs (mesh buffers, material parameters, lights, camera, sprites, UI quads) instead of `asset`/`scene` types and `core/Math.h` (its own small math header; `InstanceData`/`Particle`/`Sprite` keep their GPU layouts so the glue passes arrays as they are), the glue in `render` converting (`ModelDraw`, `TextureCache`, `Sprites`, `SceneLights`, `DrawList` with its job-system culling stay engine-side). Moves with it: `engine/shaders/` and their DXC step (the host gives the DXC path), `Renderer::Impl`, culling, the toon ramp and capture as plain pixel buffers, a hot-reload hook for `ShaderReload`. Then render goldens without the engine in MkRender's tests. Publishing it as a product is decided after.
- [ ] **MealKit-QA next candidates**: running `tests/*.scenario` with results, turning a report's recording into a scenario, `mp_test` runs.
- [ ] **UI 3 leftovers**: the old preview-tool proposal is covered by MkUI Designer; check whether it still lacks a gamepad focus walk and `--ui-check` box differences, and add them to MkUI if so.
- [ ] **UI features after v1.0** (one at a time as games need them, each checked against the 11th gen Xe budget): keyframe timeline animation (deferred table), virtualized long lists, right-to-left and complex-script shaping, per-player focus for local multiplayer, UI cost numbers (quads, layout ms) in the benchmark budget.
- [ ] **Audio features after v1.0** (in MkAudio, runtime + tool together, one at a time): bus effect chains (EQ, compressor/limiter, delay) and sidechain compression in place of volume-only ducking, master limiter + loudness meter (LUFS), clip sets loaded/unloaded per scene with a memory budget warning, a voice/profiler timeline over the live `audio/` state, dialogue lines (queue, priority, tied to subtitles), Steam Audio HRTF (deferred table).
- [ ] **Achievements, leaderboards, stats after v1.0** (`docs/DECISIONS.md` 2026-11-02): reviewed again once the platform SDK work (Steam, EOS, console trophies) is confirmed. Plan as it stood: the game defines achievement conditions as simulation events, the view side records them through the online service layer, the achievement list is one piece of data exported for each service, and without a service stats are kept in the user data folder. No effect on determinism.
- [ ] **Opt-in rendering features after v1.0** (`docs/DECISIONS.md` 2026-11-04, one at a time, each with its cost measured on minimum spec hardware and shown to the developer; view-side; additions only). Shared first piece: project settings for each feature, player quality presets (Low, Medium, High) with a per-game lock, and an editor warning when the enabled features exceed the project GPU budgets. Order: (1) project surface shaders (API and pipeline variant handling designed first), glTF occlusion texture applied to ambient light only, point light shadows on chosen lights (shadow atlas), UI blur, screen-space outlines, mesh particles → (2) post passes that don't run when off: SMAA, EASU upscaling, depth of field, motion blur, SSR → (3) RHI compute on DX12 and Vulkan, then Forward+, SSAO and GPU particles → (4) real-time GI, technique chosen when started. Deferred rendering only when a game shows a need Forward+ can't meet. What a technical artist needs around surface shaders is T6~T9 and T11 below.
- [ ] **Design refresh for the rest of the suite** (planned 2026-11-05): MealKit Manager, MkAudio and QA, which still keep their own themes; the editor's tree and table arrows, still ImGui's triangles.

---

## Editor usability (proposed 2026-11-04 and 2026-11-09, not scheduled)

Every ★ item of the 2026-11-04 review is done; what is left isn't decided. Pick items before work starts, and put open design questions in `docs/DECISIONS.md` first. Editor-only, doesn't change the shipping exe, and adds no authoring tools inside the editor (criterion ④). Game UI editing moved to MkUI Designer, so its items are MkUI's. Numbers are those of the 2026-11-04 list.

**Suggested order**: 34 (tuning A/B, = D8) → 25, 27 (asset browser, as a project's assets grow) → 21, 22, 23 (finding things in the scene tree, once scenes grow with the FuseTag maps) → 30, 31, 33 → the rest when someone asks.

- **Inspector**: 10. pinned and multiple inspectors for comparing objects, and remembering which sections are collapsed. Parts left from done items: tabs in field groups (2), drag and drop and a preview button in asset/event pickers (4), `validate` rules (6).
- **Scene tree**: 19. color tags (in the existing `.view` file), connecting lines and row stripes. 21. tree filters such as `t:spawn`, `is:hidden`, `has:problem`, and "select all of this kind". 22. selection history (`Alt+Left/Right`) and favorite objects. 23. hover link between tree rows and the viewport. Left from done items: separators (18), a prefab badge (20; objects keep no prefab link yet).
- **Asset browser**: 25. search, kind filters, favorites, a folder path bar and recently used assets. 27. preview pane: triangle count, budget bar, sound waveform and a play button.
- **Visual tools**: 30. viewport icons and labels for spawns, cameras and probes, toggled per kind. 31. small preview window for the selected virtual camera. 33. event and desync markers on the play timeline. 34. tuning A/B presets (= D8). Left from done items: `gizmo=point` (29).
- **Shared UI and workflow**: 37. rebindable shortcuts. 38. context hints in the status bar for the current tool and hovered item. Left from done items: icons with tooltips on the viewport toolbar (35).
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

**What a TA has today**: style presets, Blender material features, [Look Dev], view modes, GPU capture, `--benchmark` and post values with LUT grading. A TA can't write a shader for a game: `render::ShaderReloader` watches only `engine/shaders`, and "project surface shaders" is the first item of "Opt-in rendering features".

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
| Systems and balance design | [Data], [Balance], [Tests] | D8, D9 |
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
- [ ] R2. ★ **Installing without a build** (M). The Manager downloads or copies an engine build and registers it. Needs a published engine binary first. Open: where binaries come from and how they match a project's `engineVersion`.
- [ ] R3. **Free camera in replays** (M). Input replays and a spectator camera mode (`Free` in `scene/Scene.h`) exist, so this may be mostly connecting them, plus hiding the UI and stepping time. For trailers, screenshots and QA. Needs a DECISIONS row before v1.0. Open: shipping exe, editor or both; photo mode for players or dev-only?
- [ ] R4. **Where dialogue state lives** (decision first, S): simulation or view state decides how it syncs in multiplayer. Record it in `docs/DECISIONS.md` before any module is written.
- Later, when a game asks: R5 translation exchange (string files to and from a translation tool's format, missing keys, overflowing text); R6 store upload script around `steamcmd` after Export; R7 analytics module (needs a server, stays outside the engine).
- **Not to build**: scheduling, issue tracking and document tools; modeling, texture painting and composing tools; visual scripting and a sequencer.

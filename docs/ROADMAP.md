# MealKit engine roadmap

**Only work that is left and that an agent can build and check without a person** (anything that needs a person, a device, an account, publishing or a decision only the user can make, including work blocked by one, is in `docs/ROADMAP_HUMAN.md`; when that is resolved, the unblocked work comes back here). A finished item is removed from this document and its details go to `docs/CHANGELOG.md`; a name quoted elsewhere as ROADMAP "…" for a finished item is found there by the same name. Reasons for decisions and the "Deferred" table are in `docs/DECISIONS.md`. When the order changes, update this document.

## Where it stands (v0.9.12, 2026-11-24)

Stages 0~4 are done (v0.4~v0.8), the contract review and the cookbook are done (v0.9.0, `docs/CONTRACTS.md`), the engine is a kernel plus 20 engine module repos (`docs/ENGINE_MODULES.md`), the game UI runtime and its Designer live in MkUI (UI 2a), the repos are public (first green public CI on Linux x64 and macOS ARM64 2026-11-03), and the editor builds and runs on Linux with Vulkan (2026-11-22). **The engine stays a 0.x beta with no planned 1.0** (`docs/DECISIONS.md` 2026-11-23 "MealKit stays 0.x beta"): work is not held back for a release, and contracts change only through the rules in "Versions".

**Next, in order** (reordered 2026-11-23; the Linux editor and editor reinforcement, the 2026-11-22 decision that came first, is done: `docs/CHANGELOG.md` "check.ps1 on Linux" and "Linux editor", so game work such as FuseTag M5/M6 can resume):
1. **2D on the 3D engine**: what is left is only the "Later" items (2D lights, parallax).
2. **Game-facing gaps that FuseTag hits when game work resumes**: designer independence D4 (with FuseTag M5).
3. **UI 2**: MkUI's XAML-like layer, one piece at a time.
4. **Proposals from other engines** E1, E5, E7, E8 in their suggested order, interleaved with the picked pool items below.

Moved to `docs/ROADMAP_HUMAN.md` because they need a person, a device, an account or a decision (2026-11-24): the first beta release (its code side is done), shipped sound size and streaming (Ogg encoder choice), gamepad rumble E2 (a pad), multiplayer modes with the lobby browser, quick match and spectator slots (EOS, two PCs, Dedicated design questions), E6 (dev console vs `--query`), and stage 5.

**Pool, picked before work starts** (suggested order when nothing above is blocked): technical artist T2 → AI agents A5 → visual feedback A/C (decals, soft particles) → opt-in rendering features (1) → engine gaps G1 (proposed 2026-11-25) → Modules ⑨ (`health` first) → the rest of the tool suite rows → optimization #5~#7 only after a measured bottleneck.

## Decision criteria

Every feature is weighed in the order below to decide whether it goes in.

1. **Does it not hurt 3D + multiplayer?** Features touching the simulation must keep lockstep determinism (`docs/CODING_MODEL.md` "Determinism rules").
2. **A light default, everything else opt-in.** There is **no minimum spec** (`docs/DECISIONS.md` 2026-11-25): the target hardware is the game developer's choice. The default preset stays light (budgets below, a cost reference, not a guarantee); every heavier feature is opt-in (off by default, no cost when off, turned on per project, player-toggleable through quality presets unless the game locks them; `docs/DECISIONS.md` 2026-11-04), and a feature a device lacks is turned off on that device rather than left out of the engine. **The only rules a game must keep are the ECS structure and determinism** (criterion 1; `docs/CODING_MODEL.md` "Game object composition" and "Determinism rules").
3. **Multiple platforms, DX12 as reference.** OS APIs are called only behind the platform layer, graphics APIs only behind the thin RHI.
   New rendering features are designed on the DX12 backend first, and **the Vulkan backend is built together within the same item**. Metal, WebGPU, and console backends (stage 5) follow. Features aren't lowered to fit other APIs; features a backend can't do are turned off in that platform's preset.
   The editor runs on Windows and Linux; macOS comes in stage 5. The goal is crossplay between all platforms.
4. **Use finished things instead of building them.** The engine repo handles only the deterministic simulation (+ physics glue), lockstep networking, renderer, and editor (placement, import, checks, debugging, tool shortcuts).
   - **External authoring tools** (Blender, Penpot, VS Code, DaVinci, etc.) are used actively. They don't go into the game exe, so they cost nothing. The engine builds only import, checks, previews, and plugins for the tools.
   - **MealKit tools**: built only when no free tool fits a role's workflow. Separate repos outside the engine repo, MIT, one contract between tools ("Tool suite" below, `docs/DECISIONS.md` 2026-10-26).
   - **Runtime libraries** (linked into the exe) are also preferred over building our own. With conditions:
     freely if used only on the view side, only determinism-verified ones if they touch the simulation (like Jolt). Source included, version pinned (`third_party/`), permissive license (MIT/BSD/zlib/public domain, etc.), within the shipping exe budget (5 MB).
     Libraries that run on every target platform are looked at first. One platform's native APIs are used only inside that platform's implementation in the platform layer.

**Networking model** (fixed): host-client lockstep over UDP with input-only exchange (8 bytes per player per tick), host relay, state hashes for desync detection, host migration. Suits co-op, party, RTS, physics games and small versus with 8 or fewer players; doesn't suit competitive FPS or large player counts (every peer holds the full state, input delay 60~270 ms by ping).

### Performance budget of the default preset (reference PC, 1280×720, 60 fps)

"Current" values were measured on the reference PC, a Core Ultra 5 125U (Xe-LPG integrated graphics). The table tells developers what the default preset costs; it isn't a hardware guarantee (no minimum spec, `docs/DECISIONS.md` 2026-11-25). Opt-in features report their cost on the same PC.
This table is for Windows. Budgets for other platforms are set in a table of the same shape when that platform is started (stage 5). The 5 MB executable is the same on all platforms.

| Item | Budget | Current |
|---|---|---|
| Whole frame (GPU) | 8 ms or less (half left as headroom at 60 fps) | about 2.2 ms (arena, shadows/post on: opaque 1.33, shadow 0.39, post 0.45, 2026-10-27 `--benchmark 20`) |
| Simulation tick (30 Hz) | 4 ms or less per tick | 0.9~2.5 ms (2,000 bodies) |
| Draw calls | 200 or fewer | 26 (816 instances, arena) |
| Network (4 players, host send) | 10 KB/s or less | about 5 KB/s |
| Game executable | 5 MB or less for the default preset (excluding assets; opt-ins that add size show it) | about 3.6 MB (Export build, including audio, Jolt, post, 2026-11-03) |

## Versions

The engine version is `VERSION` at the repo root. **MealKit is a beta and stays on 0.x; no 1.0 is planned** (`docs/DECISIONS.md` 2026-11-23). Version numbers mark progress and promise nothing beyond the rules below.

- **PATCH** (0.9.9 → 0.9.10): fixes, additions, and anything that only adds keys or object kinds to existing formats.
- **MINOR** (0.9 → 0.10): a breaking change to a contract in `docs/CONTRACTS.md` (the listed public headers, `GameInstance` `kApiVersion`, file formats), or a finished stage 5 platform. Also bumps `ENGINE_VERSION` in MealKit-Docs.
- **Contracts stay stable by default**: a change prefers additions; a breaking one needs a `docs/DECISIONS.md` row first, an `upgrade_project.ps1` migration, and a CHANGELOG note on what a game must change.
- Simulation results are guaranteed only within one simulation version (unchanged from `docs/DECISIONS.md` 2026-11-03).

## Rules for every item

- **Reference samples** (`samples/sandbox` physics party versus, `samples/coop` co-op third person; 4 or fewer samples): an engine item is done only when actually used in at least one reference sample, not by unit tests alone. If a sample gets blocked by a missing engine feature, pull that item forward instead of writing workaround code in the sample. Every sample passes `check.ps1` and Export (from stage 5, on each platform). Samples aren't games: art, content and balance only as much as needed to show features.
- **When finishing**: no Release build warnings, `mp_test.ps1 -Clients 3 -Ticks 900` PASS, the default preset within budget (opt-in features: no GPU cost when off, cost measured when on), no OS/graphics API calls outside the platform layer/RHI, per-tick hashes match in every CI build, the same image on the Vulkan backend, the docs repo updated in English and Korean (`tools/build.py check` passes). New external libraries get `LICENSE` and `mealkit.txt` in `third_party/<lib>/`, then `tools\gen_notices.ps1`.
- **Format and API changes** follow "Versions": additions need nothing more; a breaking change needs its `docs/DECISIONS.md` row and upgrade migration in the same item.

---

## In progress

### [ ] 2. 2D on the 3D engine: sprite rendering + editor sprite object (decided 2026-11-10, `docs/DECISIONS.md` "2D games run on the 3D engine")
- **Done** (`docs/CHANGELOG.md` "2D projects and template cards", "Sprites on the 3D engine", "Flipbook clips"): 2D empty template (`templates/empty2d`), `view2d` front view, `PhysicsWorld::setPlane2D`, the sprite pass on DX12 and Vulkan, the `sprite` scene object (sheet cells, pivot, layer, flip, point sampling) drawn in the editor and the 2D template, sprite editing in MkRender (image picker, sheet slicer, pivot presets, the `view2d` front view; `docs/CHANGELOG.md` "Scene editing in MkRender, leftovers") and flipbook clips (`sprite_clips.table`, the sprite object's `clip` field) with their MkRender preview, and the `sprites_front` golden shot.
- **Left, in order**:
  (nothing left in the ordered list: tilemaps are done, `docs/CHANGELOG.md` "Tilemaps")
- **Rules kept**: view only, nothing enters the simulation (collision stays solid objects, or a solid flag that makes a box of the sprite's size and then enters the content hash). Budget: a few hundred sprites in one or two draws. Lit sprites only as a later opt-in.
- **Later, separate items**: 2D lights, a parallax layers helper.
- **Checks**: render golden on DX12/Vulkan (`sprites_front` exists), an MkRender capture of a sprite scene in its 2D front view, the 2D template built from scratch.

### [ ] 7. Docs (nearly done)
Left: run MealKit-Docs `tools/build.py check --engine ..\MealKit` once on a machine with Sphinx. The English pages and the Korean `.po` entries were edited by hand there (commit in MealKit-Docs); a mismatch shows up as untranslated or leftover sentences to fix with `build.py update`. Then delete this section and the "Samples out of the engine repo" heading.

## Smaller engine items

### [ ] Small leftovers
- Shipping exe size breakdown: FuseTag's Release exe is 4.63 MB of the 5 MB budget (LTCG and `/OPT:REF` already drop unused code). Measure what each library and module takes before the next runtime library is added (E8 keeps it in CI afterwards).
- Kernel memory: miniaudio allocation hooks and per-tag budgets.
- `audio` stays in the engine until its glue moves into MkAudio.
- Docs repo: `--dump-reference` reference generation and Weblate (agents already have `API.txt`, CHANGELOG "API index for agents").

### [ ] External library review follow-ups (reviewed 2026-10-29)
- **Tool repo library rules**: MkAudio, MkUI and Manager have only a README table. Align them with the engine's `mealkit.txt` + generated notices. The full SDL3 source (33 MB) lives in two repos: see whether only what the build needs can be kept.
- **Line endings**: pin `third_party/` with `.gitattributes` in the tool repos so diffs are clean when bumping versions (the ImGui backends differ only in LF/CRLF).
- **New version/CVE check tool**: a script (`tools\check_third_party.ps1`, run by hand since it needs the network) that queries upstream tags and OSV vulnerabilities from each `mealkit.txt` `source`/`version`.
- **Standing rule**: stb_image stays on `STBI_ONLY_PNG/JPEG/HDR` (`ui/StbImpl.cpp`); when adding a format, check that format's CVEs first.

### [ ] Local multiplayer on one PC (shared screen/split screen) — when the first game genre fits
- One peer provides several player slots (tick input stays 8 bytes per player); it can mix with online. Shared camera or split screen (several viewports, `buildFrame` per player), per-player UI focus, input actions assign devices to local players.
- **Determinism**: only the slot count changes. A "two players on one peer" scenario in `mp_test`. **Cost**: medium to large.

### [ ] Optimization candidates (proposed 2026-10-29) — wait for a measured bottleneck
#1~#4 and #9 are done, #8 (PGO) was measured with no gain and moved to the deferred table. The rest is done only when a measurement shows the cost it removes (`--benchmark` before and after); one that saves nothing is dropped and moved to the deferred table. On `coop --benchmark 15` (2026-11-02) the frame was 2.29 ms, GPU-bound at 2.26 of 8 ms, shadow pass 0.53 of 1.50 ms, so #5 and #7 keep waiting.

| # | Technique | Where | What it saves | Cost / risk |
|---|---|---|---|---|
| 5 | **Static shadow caching** | `Renderer::drawShadows` | Static scene objects drawn into the cascade once and copied each frame while the sun and cascade box don't move; only moving bodies drawn on top | One more depth target per cascade. Gain only with many static objects (terrain, scatter) |
| 7 | **Animation LOD** | view-side pose update (`samplePose`, `skinPalette`) | Distant or small characters update their pose every 2nd/4th frame, off-screen ones not at all (the simulation pose path is untouched) | Visible stepping up close; driven by the same screen size as mesh LOD |
| 6 | **Variable rate shading (tier 1)** | RHI + `Renderer` | Per-draw coarse shading (2×2) for the sky, distant LODs and fog-covered objects (DX12 `RSSetShadingRate`, Vulkan `VK_KHR_fragment_shading_rate`) | Toggleable, off where the device lacks it. Golden images with a tolerance. Last |

None touches `sim/`.

---

## Modules ⑨: the remaining first modules (one at a time, whenever the user wants one)

The source module system is done (`docs/MODULES.md`); `cooldown`, `objects`, `procgen`, `ai` and the combat set exist. One module = one git repo (`KingProtea99/module-<name>`, MIT), registered in `MealKit-Modules`. Suggested order: `inventory`, `minimap` (`health` is done with G2, `docs/CHANGELOG.md` "Combat framework").

| Module | sim | view/UI | Notes |
|---|---|---|---|
| `health` health/damage | hit queries (sphere, capsule, ray), damage (attacker, amount, kind), health, invulnerability ticks, death → remove Pawn → respawn wait → possess a new Pawn | health bar and damage number `.ui` | Uses the game flow framework. Hit results are processed in id order |
| `inventory` inventory | fixed-array slots, item definitions in a `.table` | grid `.ui` | Connected to saves and match start data |
| `minimap` minimap | none | top-down render or icons | View only |

---

## Tool suite (runs separately from the stages)

MealKit is a suite with the engine + general-purpose editor at the center, where small tools for workflows that differ per role are built as **separate repos** and tied together by editor shortcuts (`docs/DECISIONS.md` 2026-10-26). All free, code is MIT. Jobs that already have good free external tools stay external tool + engine plugin. Tools are split off one at a time; when to start is the user's call. Done so far: the contract between tools (MealKit-Contract), MkAudio (runtime + authoring tool), MkQA, MkUI (runtime + Designer).

| Tool (working name) | Role | Now | Direction |
|---|---|---|---|
| Level editing | Level designers | MkRender scene editing | Deeply tied to scenes, Reflect and the content hash, so it shares the engine core. Boundaries are decided when splitting |
| Data management | Designers | editor [Data] window | For the same reason, boundaries are decided when splitting |
| Code work | Programmers | VS Code, Visual Studio | An own IDE is deferred. External IDE integration first |

Items, in suggested order:
- [ ] **Opt-in rendering features** (`docs/DECISIONS.md` 2026-11-04, one at a time, each with its cost measured with `--benchmark` on the reference PC and shown to the developer; view-side; additions only). Shared first piece: project settings for each feature, player quality presets (Low, Medium, High) with a per-game lock, and an editor warning when the enabled features exceed the project GPU budgets. Order: (1) glTF occlusion texture applied to ambient light only, point light shadows on chosen lights (shadow atlas), UI blur, screen-space outlines, mesh particles → (2) post passes that don't run when off: SMAA, EASU upscaling, depth of field, motion blur, SSR → (3) Forward+, SSAO and GPU particles (RHI compute is in, G1) → (4) real-time GI, technique chosen when started. Deferred rendering only when a game shows a need Forward+ can't meet. Project surface shaders wait on the surface API decision (`docs/ROADMAP_HUMAN.md` "Blocked on a decision").
- [ ] **Design refresh for the rest of the suite** (planned 2026-11-05, approach settled by `docs/DECISIONS.md` 2026-11-12: each tool copies `applyStyle`, no shared style library). State per app is `docs/TOOL_DESIGN.md` section 7; each tool is done when the checklist in section 6 holds. App icons and the theme, icon font, icon buttons and title bar of every tool are done (CHANGELOG). Left:
  - **Tools**: a right-click item for each icon action (TOOL_DESIGN section 1; MkRender done), QA's one window on the gutter and panel layout (section 6 item 3), the Fluent icon font in place of Segoe (`docs/DECISIONS.md` 2026-11-22).
  - **Editor**: tree and table arrows, still ImGui's triangles.
  - **Tokens and text (2026-11-13, `docs/DECISIONS.md` "Suite look follows public design systems")**: the TOOL_DESIGN 3.1 colors, ramp and spacing in MkAudio, MkUI, MkRender and QA (Tokens column of section 7); QA and each tool's first screen to icon + label buttons with one primary button (MkRender done).
  - **Manager screens**: a left navigation instead of the top tabs; a project details pane (module summary with `Update all`, engine and folder under a collapsed "Details"), so the Modules tab is not the only place to see updates.
- [ ] **MkQA next candidates**: running `tests/*.scenario` with results, turning a report's recording into a scenario, `mp_test` runs.
- [ ] **UI 3 leftovers**: the old preview-tool proposal is covered by MkUI Designer; check whether it still lacks a gamepad focus walk and `--ui-check` box differences, and add them to MkUI if so.
- [ ] **UI features** (one at a time as games need them, each with its cost measured against the default preset's budget): virtualized long lists, UI cost numbers (quads, layout ms) in the benchmark budget, keyframe timeline animation (deferred table), per-player focus for local multiplayer, right-to-left and complex-script shaping.
- [ ] **Audio features** (in MkAudio, runtime + tool together, one at a time): master limiter + loudness meter (LUFS), bus effect chains (EQ, compressor/limiter, delay) and sidechain compression in place of volume-only ducking, clip sets loaded/unloaded per scene with a memory budget warning, a voice/profiler timeline over the live `audio/` state, dialogue lines (queue, priority, tied to subtitles), Steam Audio HRTF (deferred table).

---

## Editor usability (proposed 2026-11-04 and 2026-11-09, not scheduled)

Every ★ item of the 2026-11-04 review is done; what is left isn't decided. Pick items before work starts, and put open design questions in `docs/DECISIONS.md` first. Editor-only, doesn't change the shipping exe, and adds no authoring tools inside the editor (criterion ④). Game UI editing moved to MkUI Designer, so its items are MkUI's. Numbers are those of the 2026-11-04 list.

**Suggested order** (item 1 C is done): the rest of 33 → the rest when someone asks. The scene tree (19~23), inspector (10) and viewport (29~31) items moved to MkRender on 2026-11-22 (its `docs/ROADMAP.md` 4): the editor keeps only a read-only preview, so they are not editor work.

- **Inspector** (scene object inspector: MkRender's, `../MkRender/docs/ROADMAP.md` 4): item 10 and its left-over parts (tabs in field groups 2, asset/event picker drag and drop and preview 4, `validate` rules 6) moved there.
- **Scene tree** (MkRender's hierarchy, `../MkRender/docs/ROADMAP.md` 4): items 19~23, separators (18) and the prefab badge (20) moved there.
- **Asset browser** (25 and 27 done, `docs/CHANGELOG.md`): nothing left. E5's "used by" / "uses" lines go in its preview pane.
- **Tables and spreadsheets (proposed 2026-11-18)**: `.table` stays the source (text: git diff and merge, RowRef checks, content hash); Excel or Google Sheets is only for editing, as Unreal teams do with DataTable + CSV reimport, but without a binary asset. In `module-data` (`TablesPanel.cpp`, `Tables.cpp`), in this order (39 to 43 are done: `docs/CHANGELOG.md` "Tables: copy and paste of cell ranges", "Tables: CSV import preview", "Tables: Reimport", "Tables: field ranges", "Tables: Scale Selected"; left over from them: watching the file for changes, so a save in the spreadsheet offers the reimport by itself): Known Excel traps the import checks should cover: CSV is saved as cells are formatted (`0.125` shown with 2 decimals is written as `0.13`), automatic conversion of ids and values (`1-2` to a date, `1.10` to `1.1`, leading zeros), plain "CSV" saved as CP949 on Korean Windows instead of UTF-8, and the file locked while Excel has it open. No formulas in the engine: they stay in a spreadsheet.
- **Shared UI and workflow**: 37 and 38 done (`docs/CHANGELOG.md`). Left from done items: icons with tooltips on the viewport toolbar (35); status-bar hints for menu items (`commandMenuItem` has no description yet) and for the play tool.
- **From the 2026-11-09 feedback review** (build profiles are done: `docs/CHANGELOG.md` "Build profiles"; other OSes get their profiles with stage 5): Own move/scale gizmo drawing (round ends, outline, hover growth) while rotation and the axis cube stay ImGuizmo, so `third_party/imguizmo` isn't patched (now MkRender's viewport).
- **From the 2026-11-25 editor UX review** (decided 2026-11-25, `docs/DECISIONS.md`; `../EDITOR_UX_PROPOSALS_2026-11-25.md`): (a) **play changes stay**: values changed in [Tuning] and inspectors during play are kept as unsaved changes as today; the changed fields get a mark, and stopping play shows "N values changed during play: Save / Revert". Nothing reverts by itself. (b) **[Debug] split into windows**: [Console] (problems over log lines), [Profiler] and [Simulation State] become dockable windows of their own, [Window] menu grouped (Edit / Debug / Data / Preview), and a "Debug" layout preset docks them as tabs in one place for those who liked the pages, plus a "Build" preset ([Build Profiles] + [Build Output] large). Saved `layout.ini` window names move to the new names on load. Select by kind went to MkRender (`../MkRender/docs/ROADMAP.md` 4, with FuseTag M6); select by condition is deferred. Done the same day: field right-click "Find in Code", `TypeInfo::validate` checks, "Watch as Graph" on [State] rows, the [Build Profiles] and [Build Output] windows (`docs/CHANGELOG.md`).
- **Visual tools**: 33. left from the play timeline markers (shadow divergence is marked, `docs/CHANGELOG.md` "Play timeline: shadow divergence mark"): a mark for the tick where a rewind replay differs from the recording (`SimDebug::replayError` is only text, so `Timeline` needs a tick field and `kApiVersion` goes up) and simulation event ticks (decided 2026-11-24: solo play only). 30 (viewport icons and labels per kind), 31 (small preview of the selected virtual camera) and `gizmo=point` (29) moved to MkRender's view (`../MkRender/docs/ROADMAP.md` 4).

## Technical artist proposals (proposed 2026-11-04, not scheduled)

Aimed at a technical artist (TA) who owns a game's look: writing shaders, checking their cost and keeping the look from breaking. **Nothing here is decided yet**: pick items before work starts, and put open design questions in `docs/DECISIONS.md` first. Sizes are estimates. ★ marks the highest-value items.

**What a TA has today**: style presets, Blender material features, [Look Dev], view modes, GPU capture, `--benchmark` and post values with LUT grading. A TA can't write a shader for a game: `render::ShaderReloader` watches only MkRender's `shaders/`.

**Principle**: shaders are text edited in an external editor, and looks are checked and tuned in the MealKit editor. A node graph is reviewed only if surface functions have been used and TAs who don't write code are blocked by them.

**Suggested order**: T2 (small, useful with the current presets) → T5, T3 when a TA is comparing looks (T10 if an engine upgrade breaks a game's look). All are editor or tool work or debug-only shader paths, view-side. T6~T9 and T11~T14 build on project surface shaders, whose API shape is the user's decision: they are in `docs/ROADMAP_HUMAN.md` "Blocked on a decision".

### Checking a look (no project shaders needed)
- [ ] T2. ★ **Pixel inspector** (S, part). Done in MkRender (its scene view and [Look Dev], `docs/CHANGELOG.md` "Pixel inspector"). Left: the MealKit editor's own scene view and [Look Dev] page, after `tools\update_mkrender.ps1` brings the pinned MkRender to the version with `Renderer::inspectPixel`; DX12 hand check (the DX12 readback code was written on Linux and has not been built or run).
- [ ] T5. ★ **Cost comparison** (M). A page like [Debug] > [Balance] that runs `--benchmark` on a scene twice (before and after a change, or with a project setting flipped) and shows the difference per `render::GpuPass`, saying which GPU the numbers came from.
- [ ] T3. **[Look Dev] references and comparison** (M). A grey ball, a chrome ball and a color chart beside the model; a split view of two states (lighting model, HDRI, LUT or a saved image).

### Keeping a look under control
- [ ] T10. **Project look tests** (M). `--render-golden` for a game project's own shots (`tests/golden/`, `shots.txt`, an update button in [Debug] > [Tests]) on both backends. Works with style presets alone.

## Gaps against common engine features (proposed 2026-11-25, not scheduled)

From a review of terms in other teams' engine talks (Unreal, Unity, own engines) against MealKit. **Nothing here is decided yet**; pick items before work starts. Everything stays opt-in and keeps the two fixed rules, ECS and determinism (`docs/DECISIONS.md` 2026-11-25). Already present or deliberately left out (OpenGL/DX11, deferred rendering, general ECS, shader graph, particle editor, own DB/backend) aren't repeated here.

**Suggested order**: G1; G6 only after the genre question is answered.

- [ ] G1. **RHI compute**, DX12 hand check (Vulkan part done, `docs/CHANGELOG.md` "RHI compute"). Left: build and run the DX12 backend on Windows (`mkrender_compute` from the MkRender repo plus `mealkit_tests --only render`); the DX12 code was written on Linux and has not been built or run.
- [ ] G6. **Server-authoritative networking** (snapshot interpolation, dead reckoning, client prediction, dedicated server): "Not included" in the deferred table. Only if the target genre needs competitive PvP with many players or MO/MMO; first answer the dedicated server questions in `docs/ROADMAP_HUMAN.md`.

## AI agents and game developer ease (proposed 2026-11-18, not scheduled)

A review of how well an AI agent (and a developer new to MealKit) can read the engine and make a game with it. **Nothing here is decided yet**; pick items before work starts. A1 and A2 moved to "First beta release" (2026-11-23); A4 is done (`docs/CHANGELOG.md` "Shorter project agent notes").

**Already strong**: one answer per problem (`docs/CODING_MODEL.md`, the template CLAUDE.md and the cookbook teach the same patterns); mistakes caught by tools at build time with what to use instead (SimCheck `SIMnnn`, STR001, `check_layout`); every piece of game data is text (`.table`, `.tuning`, `.ui`, `.events`), so an agent edits data as well as code; self-checks without a person (`--bot`, `--capture`, `tests/*.scenario`, `mp_test`, replays); one `MEALKIT_FIELD` declaration reaches the hash, snapshots and editor panels.

**Suggested order**: A5. A6 waits on its open questions (`docs/ROADMAP_HUMAN.md`).

- [ ] A5. **More cookbook recipes** (S each). Jobs a game repeats: add a weapon (table row + RowRef + feedback row), add an enemy kind, add a game mode, add a map gimmick with `objects`. Each one is also the pattern an agent copies. English and Korean together.
- Higher-level building blocks (Modules ⑨: `inventory`, `minimap`; `health` and `ai` exist) help game developers and agents the most, and stay in that section.

## Visual feedback proposals (drafted 2026-10-29, not scheduled)

Goal: **a designer adds light and glow to an event with one `feedback.table` row**, like shake. Same rules as the existing columns: strength from `strengthFrom`/`strengthFull`, position from `positionFrom`, view side only, scaled by player settings, previewed in the [Feedback] panel. Event lights, glow kicks and hit flash are done (designer independence D2, `docs/CHANGELOG.md` "Feedback table: lights, kicks, hit flash"); nothing below is decided. The open questions become `docs/DECISIONS.md` rows when an item is picked.

**More feedback columns** (accessibility first: it guards every column that follows)
- [ ] **Accessibility**: one "flashing lights" setting multiplying `light`, `hitFlash`, `bloomKick`, `exposureKick` and `flash` (open: one slider or two), and a photosensitivity guard limiting large whole-screen brightness changes to 3 per second.
- [ ] **Object glow pulse** (`emissivePulse`, `emissivePulseSeconds`, target from `targetFrom`): the hit or activated object's emissive and rim light up and fade. Open: where the per-instance emissive scale lives (share the flash byte or add a word to the 64-byte instance).
- [ ] **Editor**: View Mode > Event Lights (each active event light as a wire sphere with its range); the preview fires `hitFlash`/`emissivePulse` at the look dev object. Open: hit flash colors other than white (instance data size).
- [ ] **Shockwave distortion** (`shockwave`, `shockwaveRadius`, `shockwaveSeconds`): a ring that bends the picture outward; needs the distortion pass (D below) and follows the shake setting.

**Shading techniques** (each a toggle; cost is a guess for the reference PC, measured with `--benchmark` before deciding; ordered by effect per millisecond; overlaps with "Opt-in rendering features" are built there)

| # | Technique | What it gives | Rough cost | Notes |
|---|---|---|---|---|
| A | Decals (projected boxes) | Scorch marks, footprints, blob shadows; events leave traces | 0.2–0.5 ms | Also a `decal` feedback column. Box projection in the forward pass via a per-instance decal list like point lights. Open: feedback-only or also placed scene objects |
| C | Soft particles | Particles stop cutting lines into surfaces | ~0.1 ms | Depth fade in the particle shader (check what `.fx` `softness` does) |
| F | Animated emissive (glTF `extras`) | Glowing runes, screens, lava without keys | ~0 | Material feature like rim; Blender add-on field |
| B | SSAO (half-res GTAO/SAO) | Objects sit on the ground | 0.6–1.0 ms | Needs scene depth; Toon can use it as a band darkener |
| D | Screen distortion pass | Heat haze, shockwaves, water refraction | 0.2 ms | A small distortion target sampled in the uber pass |
| E | Height fog + sun light shafts | Atmosphere, depth | 0.3–0.5 ms | Light shafts as a quarter-res radial blur |
| G | Spot lights + cookies | Flashlights, stage lights | like point lights | Same per-instance light table plus a cone; no shadows |
| H | Edge-detect outline (post) | Cleaner Toon outlines with inner lines | 0.3 ms | On top of the hull outline |
| I | Screen space contact shadows | Small sharp shadows near the camera | 0.3 ms | Short depth ray march toward the sun |
| J | Subsurface wrap | Softer skin and wax | ~0 | Wrap term + tint per material |
| K | Hatching | Drawn look for Toon shadows | ~0 | Driven by the toon ramp value |

Suggested order: A and C → F and B → D, then the shockwave column → the rest when a game needs them. Later opt-ins, not ruled out (no minimum spec, `docs/DECISIONS.md` 2026-11-25): SSR, TAA, froxel volumetric fog, ray tracing, mesh shaders, deferred shading.

## Roles and tools review (proposed 2026-11-04, not scheduled)

A pass over every game development role, applying criterion ④ role by role. **Nothing here is decided yet**. It comes from documents and code searches, not from running each feature, so check an item's "now" before building on it. R1 and R2 moved to "First beta release" (2026-11-23).

**Conclusion**: most roles work in external tools and need only a connection from us. The real gaps are in the work roles share: collaboration setup, installing the tools, and capture for trailers.

**How to sort a job**: judged away from our runtime → external tool (modeling, composing, scheduling); judged only inside the real game → external, and we build import, preview and checks (textures, materials, animation); tied to our data formats or determinism rules → the editor or a MealKit tool (tuning, scenes, rule data, replays).

| Role | Now | What we'd add |
|---|---|---|
| Systems and balance design | [Data], [Balance], [Tests] | (none left) |
| Level design | Scenes, terrain brushes, prefabs, D9 triggers, platforms, timers | E1 paths, E7 joints |
| Narrative | No dialogue system | R4, when a game needs it |
| Texture art | Texture cache, [Look Dev] [Materials] page | — |
| Animation | State table, animation events, 2D blend spaces, two-bone IK, visual root motion, motion warping | Sim-side root motion when a game asks (deferred table) |
| Technical art | [Look Dev] [Materials] page, proposals | T1, T2, T6~T8 |
| VFX | Own particle runtime | Mesh and GPU particles as opt-ins |
| UI/UX | MkUI Designer; Penpot optional | Nothing (Figma when designers can only use Figma) |
| Sound and music | DAW → MkAudio | "Audio features"; shipped sound size |
| Localization | Per-language string files, font glyph check, pseudo-localization in MkUI Designer | R5 |
| Build and release | D1 shared folder, Manager packages | R2 publishing, R6; D1's check on a real PC |
| Data analysis | No telemetry | R7 |
| Marketing | No capture mode | R3 |

Gameplay, engine and network programming, concept art, modeling and rigging, cinematics (no sequencer, decided), QA and production need nothing from us.

- Later, when a game asks: R5 translation exchange (string files to and from a translation tool's format, missing keys, overflowing text); R6 store upload script around `steamcmd` after Export; R7 analytics module (needs a server, stays outside the engine).
- **Not to build**: scheduling, issue tracking and document tools; modeling, texture painting and composing tools; visual scripting and a sequencer.

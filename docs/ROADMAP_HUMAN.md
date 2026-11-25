# Work that needs a person

Items from `docs/ROADMAP.md` that an agent cannot finish alone: a real device, an account or registration, a person looking at a screen, a clean PC, a decision or sign-off only the user can give, or publishing. They were moved here on 2026-11-23 and 2026-11-24 (with the code work they block) so `ROADMAP.md` lists only what can be built and checked without one. When one is done, remove it here and put the result in `docs/CHANGELOG.md`; if it unblocks code work, add that work back to `ROADMAP.md`.

Each line says **what is needed** (device, account, eyes, clean PC, decision) and which ROADMAP item it belongs to.

## Hardware (devices the user has to own and run on)

- [ ] **Intel↔AMD determinism run**: not run yet (assumed passed, 2026-10-18). (The minimum spec PC re-measurement is gone: no minimum spec since `docs/DECISIONS.md` 2026-11-25; costs are measured on the reference PC.)
- [ ] **A real gamepad**: XInput is untested with a pad; gamepad rumble (E2) is built and checked with it.
- [ ] **Real Steam connection tests** (assumed passed, 2026-10-18), **PIX capture** (code only).
- [ ] **Steam login on Linux** (ROADMAP item 1 A).
- [ ] **Cross-platform determinism between real devices**: Windows ↔ Linux/Steam Deck, Windows ↔ macOS ARM64 with `mp_test.ps1 -Peer host|join -Remote <address>`; record the results, then merge the passing groups. Windows ↔ Linux can start now that the Linux game exe exists. If it fails: revisit "Fixed-point gameplay math" in the `docs/DECISIONS.md` deferred table.
- [ ] **Crossplay**: a Windows ↔ Linux/macOS match over EOS on real devices.
- [ ] **A Mac** for the macOS editor and Metal backend (stage 5); signing and notarization need an Apple developer account.
- [ ] **Two PCs for EOS** (two processes on one PC share a device ID): joining from two PCs, relay RTT and 4-player bandwidth (budget 10 KB/s), lobby owner on host migration, an `mp_test` EOS scenario. Several online backends enabled together comes with it.
- [ ] **Windows timing** (the Linux machine can't): time `vcvars64.bat` and the CMake configure per open on the Windows editor and cache the environment if it is seconds; compare the MSVC compile with and without `-DMEALKIT_GAME_PCH=OFF` and check `/Zi` PDB sharing (CHANGELOG "Game code build time"; fewer engine headers in `app/` files was dropped on the Linux numbers, reopen only if the MSVC numbers differ). Also on Windows: with a debugger attached the PDB gets locked and hot reload builds can fail (rename the PDB per build?), and an editor configuration for the debug CRT (EditorDebug).

## Accounts, registration, publishing

- [ ] **EOS portal setup**: Epic portal product and Steam provider so Steam login and invites work through EOS.
- [ ] **First beta release** (code side done: CHANGELOG "First beta release"), **R2 publishing**: upload the engine, Manager and tool zips (Windows and Linux) to GitHub Releases and publish `index.json`; the user picks which repo holds the index (`MealKit-Releases` or `MealKit-Modules`). This is the beta release itself. Also publishing a Linux Manager zip.
- [ ] **GitHub settings check**: DCO app, branch protection on `main` (CI must pass), Discussions in `MealKit`, private vulnerability reporting, secret scanning, Dependabot alerts, topics, Pages source Actions for `MealKit-Modules`, the org `.github` repo pushed.
- [ ] **Push the remaining local commits** (after the user runs full `tools\check.ps1`), and the first run of the new CI job `linux-frontend` on GitHub.
- [ ] **Publish the template modules**: `matchflow`, `screens`, `camrig` as `KingProtea99/module-<name>` tag `v0.1.0` with entries in MealKit-Modules (repo creation under the user's account). Decide first: a view-side test build for view-only modules, or no tests for them.
- [ ] **Read the Docs** for `MealKit-Docs` and the **Windows on ARM CI runner** (both waited on publication).
- [ ] **Sponsorship** (the user is looking into it): `FUNDING.yml` in the org `.github` repo once decided. GitHub Sponsors pays out to South Korea (Stripe, W-8BEN); Open Source Collective needs about 100 stars; taxes to check first.
- [ ] **Consoles** (Xbox, PlayStation, Switch): registration in each developer program and dev kits. Code goes in a private repo (NDA).
- [ ] **A Penpot designer**: redraw the default screens in Penpot and export to `art/ui/` (was ROADMAP "Game screen UI part 3"; screens are otherwise edited in MkUI Designer; Penpot sources are saved by hand, DECISIONS 2026-10-20); also check state variants and per-token-set themes in real Penpot.

## A person looking at or clicking something

- [ ] **Drag a `gizmo=radius` handle by hand** in MkRender (ROADMAP item 1 B).
- [ ] **MkRender next to the editor at the same DPI** (TOOL_DESIGN section 6 item 7): compare the look of both windows, then the MkRender row of section 7.
- [ ] **MkRender hand editing checks and the Manager installing it** on Linux (ROADMAP item 1 A).
- [ ] **Vulkan validation layer on the Linux machine** (needs `sudo apt install vulkan-validationlayers`): then `check.ps1 -Only render` runs "Vulkan validation layer (play)" and the look dev debug layer check with the layer on instead of SKIPping (CHANGELOG "check.ps1 on Linux").
- [ ] **D5's Accept button** hasn't been clicked in a check.
- [ ] **A screenshot or play GIF for the README** (`MealKitEditor --capture`): the user picks the shot. A **short demo video**: 4 players and 8,000 physics boxes on integrated graphics.

## Clean machines

- [ ] **First-run check on a clean Windows PC and a clean Linux PC**: build from source, new project, run, export. Includes a run of the Manager with a real engine package on a clean Linux machine and the clean machine's libraries.
- [ ] **D1 on a real PC without Visual Studio, and on a network share** (designer independence); the engine and editor binaries are still copied by hand until R2.

## Decisions only the user can make

- [ ] **Ogg encoder library and quality setting** for shipped sound (DECISIONS row); shipped sound size and streaming is built after it.
- [ ] **Open design questions** the roadmap leaves for the user: Dedicated server host migration, who starts a match without a host player, one match per process or several; E6 dev console vs `--query`; A6 what an agent may change; R3 free camera shipping or dev-only; R4 where dialogue state lives; surface API shape (T6~T9 "Open design questions"); R1 Git LFS default; A1/A2 open questions.
- [ ] **Outside feedback** on the contracts (`docs/CONTRACTS.md`) and the first-run experience: needs people other than the user.
- [ ] **When to start** each tool-suite split and each Modules ⑨ module ("whenever the user wants one").
- [ ] **Whether to publish MkRender as a product of its own.**

## Blocked on a decision, an account or a device (code work waiting)

Once the blocker above is resolved, move the item back to `docs/ROADMAP.md`.

- [ ] **Multiplayer modes** (Relay, Dedicated) with the lobby browser and quick match (E3) and spectator slots (E4): Relay and the lobby browser need EOS and two PCs; Dedicated needs the open design questions answered (host migration, who starts a match, matches per process).
- [ ] **Shipped sound size and streaming**: waits on the Ogg encoder choice above.
- [ ] **Gamepad rumble (E2)**: needs a real pad to check.
- [ ] **E6 dev console**: dev console vs `--query` (with A6).
- [ ] A6. **Asking the running game and editor** (M; DECISIONS row first). Today an agent sees only screenshots and logs. A read-only query that answers in JSON: `WorldState` fields at a tick, table rows, missing RowRefs, load warnings, the content hash parts. First as a command line option on the headless exe (`--query`), then maybe an MCP server inside the editor. Open: command line only or a live server; what the editor may let an agent change (play, step, select) and what stays read-only; whether it shares E6's command table.
- [ ] R3. **Free camera in replays** (M). Input replays and a spectator camera mode (`Free` in `scene/Scene.h`) exist, so this may be mostly connecting them, plus hiding the UI and stepping time. For trailers, screenshots and QA; E4's spectators can reuse the camera. Open: shipping exe, editor or both; photo mode for players or dev-only?
- [ ] R4. **Where dialogue state lives** (decision first, S): simulation or view state decides how it syncs in multiplayer. Record it in `docs/DECISIONS.md` before any module is written.
- [ ] **Achievements, leaderboards, stats** (`docs/DECISIONS.md` 2026-11-02): reviewed again once the platform SDK work (Steam, EOS, console trophies) is confirmed. Plan as it stood: the game defines achievement conditions as simulation events, the view side records them through the online service layer, the achievement list is one piece of data exported for each service, and without a service stats are kept in the user data folder. No effect on determinism.

### Project surface shaders (technical artist T6~T9, T11~T14; waits on the surface API shape)
Also the first piece of ROADMAP "Opt-in rendering features" (1): project surface shaders, with API and pipeline variant handling designed first.
- [ ] T6. ★ **Project shader files and hot reload** (M). `assets/shaders/*.surface.hlsl`, chosen per material from the Blender MealKit panel (a glTF `extras` key in the engine schema). `ShaderReloader` also watches the project folder, and the cook compiles them, so the shipping exe needs no compiler. Used by water and lava in a reference sample before it counts as done.
- [ ] T7. ★ **Parameters declared in the shader** (M). A block at the top of the file (name, type, range, default, texture) read at compile time and shown as Blender panel fields and [Look Dev] sliders; saving goes to the glTF source through Blender.
- [ ] T8. ★ **Compile errors where the TA looks** (S). Failed reloads go to [Debug] > [Problems] and a notification, opening `file:line` in the code editor, with lines mapped back to the surface file. A broken shader keeps the previous one.
- [ ] T9. ★ **Shader budgets** (M). Project limits on texture samples and estimated instructions per surface shader from DXC's output, reported as warnings in [Debug] > [Problems] and the Blender add-on. A list shows every project shader with its pipeline count, numbers and users.
- [ ] T11. **Shared library and external editor setup** (S). An engine include with noise, UV and color helpers as part of the surface API, generated VS Code HLSL settings (include paths, defines), and a cookbook page with water and lava.

#### Later, when a game asks
- T12. Project post effects: one full-screen function in the same form as surface functions, an opt-in pass with no cost when off.
- T13. Shaders in source modules, so a look can be shared between projects. Needs the surface API settled first.
- T14. Project shaders for particles (`.fx`) and `.ui` nodes.

#### Open design questions
- Surface API shape: one `Surface surf(Input)`, or also a vertex function for waves and wind? The vertex function is hard to take back, as is every public `Input` field.
- TAs without a build toolchain: hot reload and cooking compile with DXC, which arrives with the first engine build, so a PC on the D1 shared folder may have neither. Does the editor bundle or download DXC, and where do the engine includes come from?
- Pipeline count: each surface shader multiplied by lighting model, skinned, shadow and outline pipelines, all built on load. How many are acceptable, and what does loading cost?
- Parameter storage: the material record is a fixed 128 bytes. Do custom parameters get a separate table in the upload ring?
- Later backends: surface files must go through the same stage 5 shader conversion as engine shaders.

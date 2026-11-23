# Work that needs a person

Items from `docs/ROADMAP.md` that an agent cannot finish alone: a real device, an account or registration, a person looking at a screen, a clean PC, a decision or sign-off only the user can give, or publishing. They were moved here on 2026-11-23 so `ROADMAP.md` lists only what can be built and checked without one. When one is done, remove it here and put the result in `docs/CHANGELOG.md`; if it unblocks code work, add that work back to `ROADMAP.md`.

Each line says **what is needed** (device, account, eyes, clean PC, decision) and which ROADMAP item it belongs to.

## Hardware (devices the user has to own and run on)

- [ ] **Minimum spec PC (Intel 11th gen Xe)**: re-measure the performance budget table, shadows and GPU budgets (ROADMAP "Performance budget"). Every opt-in rendering feature's cost is also measured there. Intel↔AMD determinism wasn't run (assumed passed, 2026-10-18).
- [ ] **A real gamepad**: XInput is untested with a pad; ROADMAP E2 (rumble) needs it too.
- [ ] **Real Steam connection tests** (assumed passed, 2026-10-18), **PIX capture** (code only).
- [ ] **Steam login on Linux** (ROADMAP item 1 A).
- [ ] **Cross-platform determinism between real devices**: Windows ↔ Linux/Steam Deck, Windows ↔ macOS ARM64 with `mp_test.ps1 -Peer host|join -Remote <address>`; record the results, then merge the passing groups. Windows ↔ Linux can start now that the Linux game exe exists. If it fails: revisit "Fixed-point gameplay math" in the `docs/DECISIONS.md` deferred table.
- [ ] **Crossplay**: a Windows ↔ Linux/macOS match over EOS on real devices.
- [ ] **A Mac** for the macOS editor and Metal backend (stage 5); signing and notarization need an Apple developer account.
- [ ] **Two PCs for EOS** (two processes on one PC share a device ID): joining from two PCs, relay RTT and 4-player bandwidth (budget 10 KB/s), lobby owner on host migration, an `mp_test` EOS scenario. Several online backends enabled together comes with it.
- [ ] **Windows timing** (the Linux machine can't): time `vcvars64.bat` and the CMake configure per open on the Windows editor and cache the environment if it is seconds; compare the MSVC compile with and without `-DMEALKIT_GAME_PCH=OFF` and check `/Zi` PDB sharing (ROADMAP "Game code build time").

## Accounts, registration, publishing

- [ ] **EOS portal setup**: Epic portal product and Steam provider so Steam login and invites work through EOS.
- [ ] **R2 publishing** (ROADMAP "First beta release"): upload the engine, Manager and tool zips (Windows and Linux) to GitHub Releases and publish `index.json`; the user picks which repo holds the index (`MealKit-Releases` or `MealKit-Modules`). This is the beta release itself. Also publishing a Linux Manager zip.
- [ ] **GitHub settings check**: DCO app, branch protection on `main` (CI must pass), Discussions in `MealKit`, private vulnerability reporting, secret scanning, Dependabot alerts, topics, Pages source Actions for `MealKit-Modules`, the org `.github` repo pushed.
- [ ] **Push the remaining local commits** (after the user runs full `tools\check.ps1`), and the first run of the new CI job `linux-frontend` on GitHub.
- [ ] **Publish the template modules**: `matchflow`, `screens`, `camrig` as `KingProtea99/module-<name>` tag `v0.1.0` with entries in MealKit-Modules (repo creation under the user's account). Decide first: a view-side test build for view-only modules, or no tests for them.
- [ ] **Read the Docs** for `MealKit-Docs` and the **Windows on ARM CI runner** (both waited on publication).
- [ ] **Sponsorship** (the user is looking into it): `FUNDING.yml` in the org `.github` repo once decided. GitHub Sponsors pays out to South Korea (Stripe, W-8BEN); Open Source Collective needs about 100 stars; taxes to check first.
- [ ] **Consoles** (Xbox, PlayStation, Switch): registration in each developer program and dev kits. Code goes in a private repo (NDA).
- [ ] **A Penpot designer**: redraw the default screens in Penpot and export to `art/ui/` (ROADMAP "Game screen UI part 3"); also check state variants and per-token-set themes in real Penpot.

## A person looking at or clicking something

- [ ] **Drag a `gizmo=radius` handle by hand** in MkRender (ROADMAP item 1 B).
- [ ] **MkRender next to the editor at the same DPI** (TOOL_DESIGN section 6 item 7): compare the look of both windows, then the MkRender row of section 7.
- [ ] **MkRender hand editing checks and the Manager installing it** on Linux (ROADMAP item 1 A).
- [ ] **D5's Accept button** hasn't been clicked in a check.
- [ ] **A screenshot or play GIF for the README** (`MealKitEditor --capture`): the user picks the shot. A **short demo video**: 4 players and 8,000 physics boxes on integrated graphics.

## Clean machines

- [ ] **First-run check on a clean Windows PC and a clean Linux PC**: build from source, new project, run, export. Includes a run of the Manager with a real engine package on a clean Linux machine and the clean machine's libraries.
- [ ] **D1 on a real PC without Visual Studio, and on a network share** (designer independence); the engine and editor binaries are still copied by hand until R2.

## Decisions only the user can make

- [ ] **Ogg encoder library and quality setting** for shipped sound (ROADMAP "Shipped sound size and streaming"; DECISIONS row).
- [ ] **Open design questions** the roadmap leaves for the user: Dedicated server host migration, who starts a match without a host player, one match per process or several; E6 dev console vs `--query`; A6 what an agent may change; R3 free camera shipping or dev-only; R4 where dialogue state lives; surface API shape (T6~T9 "Open design questions"); R1 Git LFS default; A1/A2 open questions.
- [ ] **Outside feedback** on the contracts (`docs/CONTRACTS.md`) and the first-run experience: needs people other than the user.
- [ ] **When to start** each tool-suite split and each Modules ⑨ module ("whenever the user wants one").
- [ ] **Whether to publish MkRender as a product of its own.**

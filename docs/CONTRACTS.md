# MealKit contracts

What a game made with MealKit can rely on from v1.0 through every v1.x. Decided in `docs/DECISIONS.md` 2026-11-03 ("v1.0 scope" and "Contract freeze scope"); the review is in `docs/ROADMAP.md` "Contract freeze review".
Until v1.0 (beta) everything here may still change. From v1.0, breaking any of it needs v2.0 and a migration in `tools\upgrade_project.ps1`.

## Game API: public headers, source compatible

- The headers in `engine/public_headers.txt` (48 today) are the game API. A game that builds on v1.0 builds on every later v1.x after a rebuild.
- Types and functions those headers declare are public, also when they come from a header that isn't listed.
- Binary compatibility isn't promised: game libraries are rebuilt for each engine version (the editor refuses a game DLL with another `kApiVersion`).
- Every other engine header is internal. `tools\check_public_headers.ps1` (check.ps1) fails when samples, templates or source modules include one; `-Path <folder>` checks a game project.
- v1.x may add headers to the list, add functions and fields, and add virtual functions with a default body to `game::GameInstance`. It doesn't remove or rename, change signatures or change what an existing call means.
- The shape a game writes: `GameSession` derives from `game::SimulationGame<World>` (`game/SimulationGame.h`) and writes `start`, `stop`, `update`, `buildFrame` (optionally `lobby`, `menus`, `debugUi`); `app/main.cpp` is `MEALKIT_GAME_MAIN`, `app/Headless.cpp` is `MEALKIT_HEADLESS_GAME`, the editor library is `MEALKIT_GAME_MODULE`.

## Files

Every text format starts with `@format <kind> <N>` after its comment header (no line = version 1, `core/TextFormat.h`). A reader skips unknown keys of a newer version with a warning and migrates older versions. In v1.x a format only gains keys, or a new `N` that still reads every older file.

| File | Format line | Version at the freeze |
|---|---|---|
| `mealkit.project` | `project` | 1 |
| `assets/game.tuning`, `assets/view.tuning` | `tuning` | 1 |
| `*.table`, `assets/input.bindings`, `assets/feedback.table` | `table` | 1 |
| `*.scene` | `scene` | 4 (reads 1~3 and old `version N` lines) |
| `assets/prefabs/*.prefab` | `prefab` | 1 |
| `*.ui`, `theme.ui` | `ui` | 1 |
| `assets/strings/<language>.txt` | `strings` | 1 |
| `assets/effects/*.fx` | `effect` | 1 |
| `assets/terrain/*.scatter` | `scatter` | 1 |
| `tests/*.scenario` | none (version 1) | 1 |
| `mealkit-tool.txt` (tool manifest) | `tool` | 1 |
| `module.txt` with `type engine` (engine module manifest, same format as source modules) | `module` | 1 |
| `modules.lock` (engine) | `module-lock` | 1 |
| `assets/sounds.events` | `audio-events` | owned by MkAudio, versioned by MealKit-Contract (`docs/CONTRACT.md` there) |

**Player-side files** live on players' PCs and outlive game versions, so they are frozen the same way:

| File (in the user folder) | Version |
|---|---|
| `saves/<name>.save` | the game's own save version (first line `# MealKit save <N>`, `game::readSave` migrate) |
| `settings.ini` | `settings` 1 |
| `input.bindings` (keys the player changed) | `table` 1 |

## Engine modules

- Engine modules are chosen by the folders in a project's `modules/` (`type engine`, `tools\module.ps1`); the 0.9.1 `mealkit.project` lines `enginePreset`/`engineModules`/`engineWithout`/`engineModuleVersion` were dropped in 0.9.2 (`upgrade_project.ps1` converts them).
- A game builds against an engine version (`VERSION` + `modules.lock`), never against module versions one by one. A module's public headers are part of the game API when they are in `engine/public_headers.txt`.
- `#if MEALKIT_HAS_<NAME>` is how game code checks for an optional module, and `game::MatchService::multiplayerAvailable()` tells a menu whether this build has `net`.

## Simulation rules

- The determinism rules in `docs/CODING_MODEL.md` "Determinism rules (simulation code)".
- SimCheck errors `SIM001`~`SIM011` and `SIM013`~`SIM015` (`engine/tools/SimCheck.cpp`). Rules added in v1.x are warnings only, like `SIM012`; turning one into an error waits for v2.0, so an engine update never stops a game's build.
- The simulation state rules: fields declared with `MEALKIT_FIELD` in `WorldState`, only `f32/i32/u32/bool/Vec3/Quat/Id<T>/physics::BodyId`/fixed arrays, no strings, pointers or `vector`.

## Not part of the contract

- **Simulation results across engine versions.** A v1.x update (a Jolt version or a physics fix, for example) may change what the same inputs produce. Results are the same only within one simulation version (engine and game `sim/` sources, `mealkit_build_info.h`), which is also what replays and crossplay match on.
- Replays (`.rec`, version 3) recorded by another simulation version, cooked pack files (`.pak`, rebuilt by Export), baked probes (`.mkprobe`), desync dumps, logs and `build/`.
- Engine internals outside the public headers, the editor's own files (layouts, preferences), command line options of tools.

# @NAME@

A game project made with the MealKit engine (`@MEALKIT_ROOT@`, 3D multiplayer, Windows + DirectX 12 (reference)/Vulkan, C++20/MSVC/CMake+Ninja).
Talk with the user in Korean. Code comments, log messages, and scene file keys are written in English; editor UI text is English source text with a Korean translation.

## Commands

- Build game code (DLL for the editor, hot reload): `build.bat Editor` → `build\Editor\bin\@NAME@_game.dll`
- Shipping builds: `build.bat` (Release) / `build.bat Debug` / `build.bat Profile` / `build.bat Export` (→ `export\@NAME@\`)
- Run: `build\Release\bin\@NAME@.exe --help` (game), `editor.bat` (editor)
- Determinism test: `powershell -ExecutionPolicy Bypass -File @MEALKIT_ROOT@\tools\mp_test.ps1 -Clients 3 -Ticks 900 -Exe build\Release\bin\@NAME@.exe` → must PASS
- Check the game screen: `build\Release\bin\@NAME@.exe --bot --exit-tick 300 --capture out.png 240`
- Check the editor screen: `@MEALKIT_ROOT@\build\Editor\bin\MealKitEditor.exe --project . --frames 60 --select 5 --capture out.png` (add `--play --bot` for play mode)

## Rules

- Engine code and the editor aren't in this folder. If the engine needs changes, change it in `@MEALKIT_ROOT@` (following its CLAUDE.md).
- `source/sim/` is deterministic code: no CRT trig functions (use `mealkit/core/DetMath.h`), keep `/fp:precise`, use only `Pcg32`, fixed iteration and body creation order.
  State that persists across ticks is declared with `MEALKIT_FIELD` inside `WorldState`. Once declared, it automatically goes into the hash, snapshots, and the editor [State] panel.
  SimCheck checks at build time, and violations stop the build with `error SIMnnn` (it tells you what to use instead). The reasoning behind the rules is in the engine's `docs/CODING_MODEL.md`.
  After changing simulation code, confirm there are no desyncs with the determinism test.
- Rule tests: `tests/*.scenario` (seed or input recording + `tick 300 slots[1].falls == 1` checks on `WorldState` fields) run with `build\Release\bin\@NAME@_headless.exe --test tests\x.scenario` (`--update-expected` after changing results on purpose). Editor play bar [Save Scenario] makes one from solo play.
- Game code is in a namespace named after the project in lower case (`tools\rename_project.ps1` renames it with the project).
- Game code folders: `source/sim/` (rules: one file per system; system functions take only `World&` and `World::step()` calls them in order),
  `source/view/` (camera, local input, drawing; only reads sim state), `source/app/` (`GameSession` = the `mealkit::game::GameInstance` implementation, `MEALKIT_GAME_MAIN` in `main.cpp` (shipping exe; main, window, and session are in the engine's `mealkit/game/GameMain.h`), `MEALKIT_GAME_MODULE` in `GameModule.cpp`).
  In `sim/`, objects created/destroyed during a match use `mealkit::sim::Pool<T>`, "N ticks later" uses `TickTimer`, and things to tell the view go through `world.events` (`TickEvents`).
  The view side takes engine services as `mealkit::game::GameContext&` (`docs/CODING_MODEL.md`).
- People (`PlayerSlot`) and bodies (`Pawn`) are separate, connected by a `Controller` (player or AI). Respawn, spectating, mounting, and AI are implemented by creating new Pawns or switching with `possess`/`unpossess` (`source/sim/Pawns.cpp`).
- `GameSession` runs ticks with the engine's `game::Simulation<World>` (shadow simulation, pause, and rewind come from here). Don't write your own tick loop.
- New object kinds to place in the editor are declared in `source/sim/Objects.h` as `MEALKIT_OBJECT(Name) { MEALKIT_FIELD(type, field, default, "Display name"); };`,
  and read in `World.cpp`'s `loadScene` with `scene::readObject<Name>(entity)`. Rebuilding game code makes them appear in the editor [Add] menu.
  Includes are written relative to `source/`, like `"sim/World.h"`. Adding files doesn't require editing `CMakeLists.txt`.
- **Don't keep numbers as code constants** (engine `docs/CODING_MODEL.md` "Where to put numbers"): game rule values → `Tuning`; values that differ per map (ranges, counts, heights) → scene objects (areas are the position/size of a `MEALKIT_OBJECT` with `kObjectFlags = mealkit::reflect::kObjectArea`),
  visual-only values → `ViewTuning`. SimCheck warns on float literals in `sim/` (SIM012). Only unit conversions and algorithm constants are kept, with `// const-ok: <reason>` at line end.
- Tuning values (numbers like speeds and cooldowns) are declared with `MEALKIT_FIELD` in `source/sim/Tuning.h` instead of code constants and read via `world.tuning`. Values are edited in `assets/game.tuning` and the editor [Tuning] panel.
  Visual-only values (camera, colors, effect durations) go in `source/view/ViewTuning.h` (`assets/view.tuning`, the "View" tab of the [Tuning] panel) and are read on the view side with `ctx.viewTuning<ViewTuning>()`. They aren't in the content hash, so they change immediately even during multiplayer.
  Time values are declared as `MEALKIT_FIELD(mealkit::Ticks, name, mealkit::Ticks::seconds(0.5f), "Description (seconds)")` (seconds in files and the editor, integer ticks in `sim/`).
- Data with many rows of the same shape (weapons, items, enemy kinds) uses definition data tables: row type `MEALKIT_STRUCT` and `MEALKIT_TABLE(Row, "tables/x.table", 0)` in `source/sim/Tables.h`,
  values in `assets/tables/x.table` (editor [Tables] panel, CSV). Other data points to rows with `mealkit::RowRef<Row>` (row id). `startWorld` in `app/Headless.cpp` reads them with `mealkit::data::loadTable` and passes them to the `World`, and `sim/` reads by row index.
- After upgrading the engine, accept when the editor asks to "upgrade project" (or `@MEALKIT_ROOT@\tools\upgrade_project.ps1 -Project .`).
  It replaces only build files and leaves files you edited as `<file>.new`. Don't delete `.mealkit-template`.
- The engine decides the folder layout: directly under `assets/` only kind folders (`scenes models textures sounds effects ui fonts tables strings lut`) and fixed files; external tool sources (`.blend`, `.psd`, Penpot) go in `art/`.
  External libraries get attached to the game exe/DLL when placed in `third_party/<lib>/` with the original `LICENSE` and a `mealkit.txt` (name, version, license, source, usedIn, and `cmakeTarget` if there's something to build) (SIM011 in `sim/`).
  `@MEALKIT_ROOT@\tools\check_layout.ps1 -Project .` checks it, and Export fails with the same check.
- Things that change the simulation, like debug cheats and mid-match settings changes, aren't done by modifying state from the view side but sent as confirmed commands: a command type `MEALKIT_STRUCT` (no strings) in `source/sim/`, `mealkit::sim::sendCommand(ctx.session, command)` on the view side, processed with `mealkit::sim::forEachCommand` at the very start of `World::step()` (`mealkit/sim/Commands.h`).
- Dev screens (performance, cheat buttons, state views) are drawn with Dear ImGui inside `GameSession::debugUi(ctx)`, wrapped in `#if MEALKIT_DEBUG_UI` (F1 in the game exe, during play in the editor). They're absent in shipping builds (`build.bat Export`). Not used for game UI.
- Game screens (HUD, menus) are written in `assets/ui/*.ui` (previewed in the editor [View] > [UI], re-read immediately on save), and game code keeps `MEALKIT_VIEWMODEL` view models in `source/view/` and does `ctx.ui.open("ui/hud.ui")` → `ctx.ui.bind(screen, vm)` → fill vm every frame; buttons via `ctx.ui.actions()`. If UI needs to change the simulation, send confirmed commands.
- Player settings (display, sound, keys, language) use the engine's standard settings screen: `button settings.open` on a button, `ctx.settings.open()` in code, values via `ctx.settings.values()` (view side only; not read in `sim/`). Change the look in `assets/ui/settings.ui`, keeping the bindings and `settings.*` actions.
- Values that affect a match, like each player's chosen character and gear, are sent as loadouts (a `MEALKIT_STRUCT` in `source/sim/Loadout.h`): `mealkit::game::setLoadout(session, l)` in `GameSession::lobby`, read with `mealkit::game::readLoadout(match, p, l)` in `Headless::startWorld`, and `World::reset` copies them into player slots (state). `sim/` reads only that copy. People joining mid-match arrive via engine commands: in `applyCommands`, `mealkit::sim::readPresence(c, p)` → for a new slot, create the slot and controller and `p.readLoadout(l)`; on disconnect (`!p.connected`), remove that person's character (sandbox `setPresence`).
- Screen flow (title → menu → lobby → match → results → lobby) is `source/app/Flow.cpp` and the screen `.ui` files in `assets/ui/`. Opening and closing rooms only calls `ctx.match` (`host`, `join`, `playOffline`, `start`, `returnToLobby`, `leave`), and the match end is decided by the match phase in `sim/`: `mealkit::sim::MatchFlow` (`WorldState.flow`) advanced by `source/sim/Match.cpp` with rules from tuning (`roundLength`, `rounds`, `scoreLimit`, `lives`, `teams`, ...); react to rounds in `onRoundStart`/`onRoundEnd` there.
- Records and unlocks that persist outside matches are saved as one `MEALKIT_STRUCT(SaveData)` in `source/app/SaveData.h` with `mealkit::game::writeSave(m_save, ctx.userFolder, "progress", kSaveVersion)`/`readSave`. Old files are read even when fields are added or removed, and when meanings change, bump the version and convert in migrate. `sim/` doesn't read saves.
- Player-visible text isn't written in code; it goes in `assets/strings/ko.txt` (`key = string`, one file per language) and is read with `ctx.strings.get("key")`, or with values `ctx.strings.format("key", {{"name", value}})`. If table characters aren't in the fonts in `assets/fonts`, the build stops (STR001).
- Controls are read as input actions, not keys: `ctx.actions.axis2("move")`, `ctx.actions.pressed("jump")`. Actions and key/mouse/gamepad bindings live in `assets/input.bindings` (editor [Tables] panel), and code doesn't look at keys (`window.input()`) directly.
- Sound, shake, flash, and screen freeze attached to tick events (`world.events`, `GameEvent` kinds are `MEALKIT_ENUM`) go in the feedback table `assets/feedback.table` (editor [Feedback] panel), not code. Game code only calls `ctx.feedback.trigger(e, localPlayer)`.
- Sound is played only from `source/view/` with `ctx.audio.play("event", position, {{"parameter", value}})` (`mealkit/audio/Audio.h`). Events (clips, volume, attenuation, concurrent play limits) and buses are in `assets/sounds.events` (authoring tool MealKit Audio, editor [Tools] menu; preview in the [Sound] panel),
  clips are WAVs in `assets/sounds/` (music uses `stream true`). `sim/` doesn't know sound and only leaves tick events in `world.events`.
- When scene collision data, model files, or tuning values change, the content hash changes, and peers using different data are refused connection.

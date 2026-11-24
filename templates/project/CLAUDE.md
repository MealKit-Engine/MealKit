# @NAME@

A game project made with the MealKit engine (`@MEALKIT_ROOT@`, 3D multiplayer, Windows + DirectX 12 (reference)/Vulkan, C++20/MSVC/CMake+Ninja).
Talk with the user in Korean. Code comments, log messages, and scene file keys are written in English; editor UI text is English source text with a Korean translation.

## Commands

- Build game code (DLL for the editor, hot reload): `build.bat Editor` → `build\<os>\Editor\bin\@NAME@_game.dll`
- Shipping builds: `build.bat` (Release) / `build.bat Debug` / `build.bat Profile` / `build.bat Export` (→ `export\<os>\@NAME@\`)
- Run: `build\<os>\Release\bin\@NAME@.exe --help` (game), `editor.bat` (editor)
- Determinism test: `powershell -ExecutionPolicy Bypass -File @MEALKIT_ROOT@\tools\mp_test.ps1 -Clients 3 -Ticks 900 -Exe build\<os>\Release\bin\@NAME@.exe` → must PASS
- Check the game screen: `build\<os>\Release\bin\@NAME@.exe --bot --exit-tick 300 --capture out.png 240`
- Check the editor screen: `@MEALKIT_ROOT@\build\Editor\bin\MealKitEditor.exe --project . --frames 60 --select 5 --capture out.png` (add `--play --bot` for play mode)

## Core rules

1. The engine and the editor aren't in this folder. If the engine needs a change, make it in `@MEALKIT_ROOT@` (following its CLAUDE.md).
2. `source/sim/` is deterministic: no CRT trig (use `mealkit/core/DetMath.h`), `Pcg32` only, fixed iteration and creation order, persistent state is a `MEALKIT_FIELD` inside `WorldState`. SimCheck stops the build with `error SIMnnn` and says what to use instead. After a change in `sim/`, run the determinism test.
3. `source/sim/` (rules), `source/view/` (drawing, local input; only reads sim state) and `source/app/` (`GameSession`) stay apart. The view changes the simulation only by sending confirmed commands.
4. No numbers as code constants: rule values go in `Tuning` (`assets/game.tuning`), per-map values in scene objects, visual-only values in `ViewTuning`.
5. Many rows of the same shape (weapons, items, enemies) are a `.table` with `RowRef`, not code.
6. Player-visible text is in `assets/strings/`, controls are input actions (`assets/input.bindings`), screens are `assets/ui/*.ui`, and sound and shake are table rows (`assets/sounds.events`, `assets/feedback.table`). None of it is in code or `sim/`.
7. Use what the engine has: `SimulationGame` (no own tick loop), `Mover` (no own character controller), the settings screen, and the source modules `screens`, `matchflow`, `camrig`.
8. Build through the commands above. Template files you haven't edited are replaced by the editor's "upgrade project"; edited ones get `<file>.new`. Don't delete `.mealkit-template`.
9. The engine decides the folder layout (`assets/` has only kind folders, tool sources go in `art/`, libraries in `third_party/<lib>/` with `LICENSE` and `mealkit.txt`): `@MEALKIT_ROOT@\tools\check_layout.ps1 -Project .`.
10. A change to scene collision, models or tuning changes the content hash, and peers with different data are refused.

## When doing X, read Y

| Job | Read |
|---|---|
| New code, new object kinds, modules, upgrading, folder layout | `docs/AGENT_NOTES.md` "Code layout and engine parts" |
| Anything in `source/sim/`, scenarios, the content hash, commands from the view | `docs/AGENT_NOTES.md` "Determinism and tests" |
| A number, a speed or cooldown, a list of weapons or items | `docs/AGENT_NOTES.md` "Numbers and data tables" |
| HUD, menus, settings, dev screens, title/lobby/results flow | `docs/AGENT_NOTES.md` "Screens and UI" |
| Loadouts, character movement, progress saved between matches | `docs/AGENT_NOTES.md` "Match values, movement and saves" |
| Player-visible text, controls | `docs/AGENT_NOTES.md` "Text and input" |
| Shake, flash, hit stop, sound | `docs/AGENT_NOTES.md` "Feedback and sound" |
| A recipe for a job games repeat | the engine cookbook (MealKit-Docs cookbook pages, `@MEALKIT_ROOT@\docs\CONTRACTS.md`) |
| A source module (`screens`, `matchflow`, `camrig` ...) | `modules/<name>/docs/index.md` |
| Why a rule exists | the engine's `docs/CODING_MODEL.md` |

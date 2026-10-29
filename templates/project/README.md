# @NAME@

A game project made with the [MealKit engine](@MEALKIT_ROOT_FWD@/README.md).
The engine and editor are in `@MEALKIT_ROOT@`; this folder holds only **game code, assets, and build output**.

## Folders

| Folder/file | What |
|---|---|
| `mealkit.project` | Project settings: name, engine location (`engine`), start scene (`scene`), game items (`game.*`). Edited in the editor via [File] > [Project Settings] |
| `source/` | Game code. `sim/` (deterministic game rules, checked by SimCheck at build time), `view/` (camera, controls, drawing), `app/` (`GameSession` runs a game, `main.cpp` the shipping exe (one line `MEALKIT_GAME_MAIN`, the rest is the engine), `GameModule.cpp` the DLL entry point for the editor) |
| `assets/` | **Only what ships.** Directly under it, only kind folders (`scenes/ models/ textures/ sounds/ effects/ ui/ fonts/ tables/ strings/ lut/`) and fixed files (`game.tuning`, `view.tuning`, `input.bindings`, `feedback.table`, `sounds.events`). Subfolders inside kind folders are free |
| `art/` | External tool sources (`.blend`, Penpot, PSD, sound projects). Not included in shipping or the content hash |
| `modules/` | Source modules (stage 4) |
| `third_party/` | External libraries the game uses. Each `<lib>/` has the original `LICENSE` and a `mealkit.txt` (name, version, license, licenseFile, source, usedIn, kept). Can't be used in `sim/` |
| `build/Editor/` | Game code `@NAME@_game.dll` loaded by the editor |
| `build/Release/` | Shipping `@NAME@.exe` (determinism tests, running directly) |
| `export/@NAME@/` | **The shipping build.** Give this whole folder to friends or upload it to Steam |

## Workflow

1. Open the editor with `editor.bat` (the engine's `MealKitEditor.exe` opens this project). The first time, wait a moment while game code builds.
   Running the engine editor by itself shows the **project manager**, where you can pick a recent project.
2. Open this folder in VS Code and edit game code **with the editor left open**.
3. `Ctrl+Shift+B` (build game code) → the editor detects the new DLL and **swaps it in automatically** (hot reload). Pressing `Ctrl+R` in the editor works too.
   The scene, camera, and selection stay as they are. During play, it switches after stopping.
4. Scene editing and `F5` play happen in the editor. During editor play, the same game code runs as a DLL.
5. **Shipping build**: editor `Build → Shipping Build` (`Ctrl+B`) or `build.bat Export` → `export\@NAME@\`.
6. Checking multiplayer: `Tasks: Run Task` → `Multiplayer Determinism Test` (must PASS).
7. Debugging: VS Code `F5` → `Editor (debug game code)` sets breakpoints in game code inside the editor.

## Rules

- `source/sim/` is lockstep deterministic code. Follow "Determinism rules" in the engine's `docs/CODING_MODEL.md` (no CRT trig functions, only `Pcg32`, fixed iteration order, new state into `hash()`).
- Multiplayer connects only when every player has **the same exe, the same scene, and the same model files**. Share the whole shipping folder.
- The Steam app ID is `STEAM_APP_ID` in `CMakeLists.txt` (default 480 = Valve's test app). With your own app ID, `steam_appid.txt` isn't put in the shipping folder (Steam launches it).
- Engine changes are made in the engine folder (`@MEALKIT_ROOT@`). When the engine changes, the editor rebuilds game code.
- The engine decides the folder layout. `@MEALKIT_ROOT@\tools\check_layout.ps1 -Project .` finds misplaced files (Export fails with the same check), and the editor warns when opening the project.

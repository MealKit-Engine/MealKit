# Tool design rules

How the MealKit editor and the suite tools (MealKit Manager, MkAudio, MkUI, MkRender, MealKit-QA) look and behave, so they read as one product.
Decision: `docs/DECISIONS.md` 2026-11-12 "One look for the editor and the tools, icons over words".

## 1. Icons over words (the Blender way)

The screen shows icons; words go in tooltips. A row of actions is a row of icon buttons, not a row of labelled buttons.

- **Icon buttons are square and flat**: clear until hovered, filled with the selection color while a toggle is on (editor `iconButton`, `iconValueButton` for an icon with a count; Manager `App::iconButton`).
- **Icon-only buttons** for toolbars, row actions, panel headers and the status bar. Each has a tooltip with its name, plus its shortcut when it has one (`Open in Editor  Enter`), and the tooltip still shows while the button is disabled. A tooltip may add one more line that says why it is disabled or what it keeps (`Files you changed are kept as .new`).
- **Words stay** where an icon would make people guess:
  - the confirm and cancel buttons of dialogs (`Create`, `Remove`, `Cancel`),
  - menu items: right-click and menu bar items are where people look things up, so they show an icon and the name,
  - a count or a state that is the information itself (`7`, `0.9.2 -> 0.9.4`, `folder missing`),
  - an action with no glyph that clearly fits. Leave it as text instead of inventing an icon.
- **The same action uses the same glyph in every tool** (table below). When a tool needs a new one, add it to the table first.
- **Every icon action has a second way in**: a right-click menu item with its name, and a shortcut for the common ones. Then icon-only buttons are never the only place an action can be found.
- **Without the icon font** (another OS, the font is missing), the same buttons fall back to their names. Code builds the button from (glyph, name) and never from the glyph alone.
- **Explanations go in tooltips and empty states, not in permanent lines.** Leave out "Right-click for more"-style hints and paragraphs above tables.

## 2. Glyphs

Windows: Segoe Fluent Icons, falling back to Segoe MDL2 Assets, merged into the UI font at the same size. Write glyphs as UTF-8 escapes with the code point in a comment, for example `"\xEE\x9C\x90" // U+E710`.

| Action | Glyph | Code point |
|---|---|---|
| New, add | Add | U+E710 |
| Close, cancel, dismiss | Cancel | U+E711 |
| More | More | U+E712 |
| Settings | Setting | U+E713 |
| Search | Search | U+E721 |
| Refresh, check again | Refresh | U+E72C |
| Favorite off / on | FavoriteStar / FavoriteStarFill | U+E734 / U+E735 |
| Remove from a list (nothing deleted) | Remove | U+E738 |
| Delete files | Delete | U+E74D |
| Upgrade, move up | Up | U+E74A |
| Edit, rename | Edit | U+E70F |
| Play, run, resume / pause / stop | Play / Pause / Stop | U+E768 / U+E769 / U+E71A |
| Step one tick | Next | U+E893 |
| Save | Save | U+E74E |
| Undo, revert / redo | Undo / Redo | U+E7A7 / U+E7A6 |
| Clear a log | Clear | U+E894 |
| Accept | Accept | U+E73E |
| Open a web page (repository) | Globe | U+E774 |
| Capture (GPU frame, screenshot) | Camera | U+E722 |
| Set as the start scene | FavoriteStarFill | U+E735 |
| Copy, copy path | Copy | U+E8C8 |
| Open in its program (editor, tool) | OpenInNewWindow | U+E8A7 |
| Folder, import a folder, add a folder | Folder | U+E8B7 |
| Show in Explorer | FolderOpen | U+E838 |
| Install, download | Download | U+E896 |
| Update to a newer version | Sync | U+E895 |
| Build | Repair | U+E90F |
| Output, log | Document | U+E8A5 |
| Information / warning / error | Info / Warning / Error | U+E946 / U+E7BA / U+EA39 |
| Visible / hidden, lock / unlock | View / Hide, Lock / Unlock | U+E890 / U+ED1A, U+E72E / U+E785 |

Kind icons (module keywords, scene object kinds) are listed with their code: `mkcontract::moduleIconKeyword` and module-editor `ImGuiLayer.h` `icon::`.

## 3. Look

The values live in one place, module-debug `src/mealkit/debug/ImGuiLayer.cpp` `applyStyle`. A tool that doesn't link the engine copies that function, names its source in a comment, and gets updated whenever the original changes.

- **Neutral dark layers**: a gutter (`#131416`-ish, also the window title bar color through DWM) behind panels (`#1E1F22`-ish). Panels are set apart by space and the darker gutter, not by lines.
- **Tabs are flat**: no fill and no underline. The selected tab joins its panel. The tab strip and the status bar sit on the gutter.
- **The accent blue is only for selection, focus and active states.** No colored button fills, including the main action.
- **State colors**: amber `(1.0, 0.8, 0.3)` for "needs an action" (upgrade, not installed), red `(1.0, 0.45, 0.4)` for errors and missing things. Use them for text and icons (the output icon of a failed job is red), never as button fills.
- **Secondary text is dimmed** (`TextDisabled`): paths, dates, authors, hints.
- **Font**: Segoe UI with Malgun Gothic merged for Korean, 16 px before DPI scale.
- Thin 10 px scrollbars, 4 px frame rounding, the padding and spacing of `applyStyle`.

## 4. Behaviour

- **Lists**: click selects, double-click or Enter opens, Delete removes, the arrow keys move the selection. A right-click menu has the row's actions. Column headers sort where order matters. A truncated path shows in full in its tooltip.
- **Empty states**: one sentence that says what is missing, and the button that fixes it, in the middle of the space. No empty tables.
- **Search**: Ctrl+F focuses it, Esc clears it, it shows `matches / total` while filtering, and a line says when nothing matches.
- **Long jobs**: show a turning arc with the job name and the seconds so far. A failed job leaves a short status line and an output button with Copy.
- **Destructive actions** ask first in a small dialog that says what stays (`the folder is kept`, `undo with version control`). Taking a row off a list doesn't ask.

## 5. App identity

Every suite app is recognisable on the taskbar, in the Start menu and on the desktop, not only once it is open.

- **The exe carries its own icon** as resource `1 ICON` in a `.rc` with `#pragma code_page(65001)`, like the editor's `engine/res/MealKitEditor.rc`. Sizes 16, 20, 24, 32, 40, 48, 64 and 256 px in one `.ico`, made by a script (the editor's `tools/make_editor_icon.py`, the tools' `tools/make_suite_icons.py`; a new tool adds its mark there), never drawn by hand per size.
- **The window uses the same icon** (SDL apps: Windows takes the class icon from resource 1, so nothing else is needed; check the taskbar and Alt+Tab).
- **Shortcuts and the Installed apps entry point at the exe's icon** (`<exe>,0`, Manager `install.ps1`). An exe without an icon resource shows the blank Windows app icon there.
- **One family**: the same rounded square and background as the editor's icon, one glyph or mark per app in its own color, so the apps read as one suite side by side.
- **Title bar**: dark, in the gutter color `#131416` through DWM (`DWMWA_USE_IMMERSIVE_DARK_MODE`, `DWMWA_CAPTION_COLOR`; Manager `applyEditorCaption`), never the light system bar.

## 6. Bringing a tool to this look

A tool is on the suite look only when every line holds; until then it is on the list in section 7.

1. `applyStyle` copied as `applyEditorStyle`, with a comment naming module-debug `ImGuiLayer.cpp` as its source. Differences are allowed only where the tool's layout needs them, each with a comment (Manager: no window border, `WindowBg` is the gutter and content sits on panel-colored children).
2. Fonts: Segoe UI 16 px + Malgun Gothic merged + the icon font merged (section 2), all at the DPI scale.
3. Layout: content on panels over the gutter, tab strip and status bar on the gutter; no full-width ImGui window title bars inside the OS window.
4. Buttons: icon buttons from (glyph, name) per section 1, glyphs from the table in section 2.
5. Title bar and app icon per section 5.
6. A screenshot of the tool beside the editor at the same DPI shows the same colors and spacing.

When `applyStyle` changes, the same change goes to every copy listed in section 7 in the same piece of work.

## 7. Where each app stands (2026-11-12)

| App | Theme | Icon font, icon buttons | Title bar | App icon |
|---|---|---|---|---|
| MealKit Editor | `applyStyle` (source) | yes | DWM | yes (`make_editor_icon.py`) |
| MealKit Manager | copy in `src/main.cpp` | yes | DWM | yes |
| MkAudio | copy in `tool/main.cpp` | yes (panel buttons; panel tabs without icons yet) | DWM | yes |
| MkUI Designer | copy in `tool/main.cpp` | yes (panel buttons; panel tabs without icons yet) | DWM | yes |
| MkRender | copy in `tool/main.cpp` | yes (panel buttons; panel tabs without icons yet) | DWM | yes |
| MealKit QA | `StyleColorsDark` + 3 px rounding | no | system | yes |

The work left is ROADMAP "Design refresh for the rest of the suite". Update this table when a tool moves.

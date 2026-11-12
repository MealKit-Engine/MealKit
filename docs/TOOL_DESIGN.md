# Tool design rules

How the MealKit editor and the suite tools (MealKit Manager, MkAudio, MkUI, MkRender, MealKit-QA) look and behave, so they read as one product.
Decisions: `docs/DECISIONS.md` 2026-11-12 "One look for the editor and the tools, icons over words" and 2026-11-13 "Suite look follows public design systems".
Sources: Fluent 2 (sizes, type ramp, 4 px grid), Radix Colors (12-step scales), GNOME HIG (progressive disclosure), Blender HIG and the IntelliJ Platform UI Guidelines (editor tools), Toss's writing (Korean text), Mozilla Fluent (localization). Research note: workspace `UX_DESIGN_REFERENCES_2026-11-13.md`.

## 1. Icons and words

Two kinds of screens:

- **Work screens** (the editor, the panels and panel toolbars of MkAudio, MkUI, MkRender): people spend hours here and learn the icons. A row of actions is a row of icon buttons, words go in tooltips (the Blender way, the rest of this section).
- **Occasional screens** (the Manager, QA, a tool's first screen and empty states, dialogs): people open them now and then. Actions are **icon + label** buttons, and each screen has **one main action** drawn as the accent-filled primary button (`Open in Editor`, `New Project`, `Create`). Row actions that are rare (copy path, remove from list) stay in a `More` button and the right-click menu.

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

### 3.1 Tokens

Every size and color comes from these tables. Pick by the step's job, never a color by eye.

**Neutral scale** (Radix `slateDark`), **accent** (Radix `blueDark`):

| Step | Value | Job | ImGui |
|---|---|---|---|
| gray 1 | `#111113` | gutter: behind panels, tab strip, status bar, title bar (DWM) | `WindowBg` of the outer window, `MenuBarBg`, `TitleBg*` |
| gray 2 | `#18191B` | panels, cards | `ChildBg` / panel `WindowBg`, `TabSelected` |
| gray 3 | `#212225` | control at rest, popups | `FrameBg`, `Button`, `PopupBg` |
| gray 4 | `#272A2D` | **hover** (also of clear buttons and rows) | `*Hovered`, `HeaderHovered`, `TabHovered` |
| gray 5 | `#2E3135` | pressed, selected row | `*Active`, `Header` |
| gray 6 | `#363A3F` | lines, borders | `Separator`, `Border`, table borders |
| gray 7 | `#43484E` | border of a hovered field, scrollbar grab | `ScrollbarGrab*` |
| gray 9 | `#696E77` | disabled text | `TextDisabled` in tools without secondary text |
| gray 11 | `#B0B4BA` | secondary text: paths, dates, hints | `TextDisabled` |
| gray 12 | `#EDEEF0` | text | `Text` |
| blue 8 | `#2870BD` | focus ring, selection bar | `NavCursor`, the 3 px bar left of a selected row |
| blue 9 / 10 | `#0090FF` / `#3B9EFF` | primary button / its hover; check marks | primary button, `CheckMark`, `SliderGrabActive` |

State colors stay as text and icon colors: amber `#FFCA16` (needs an action), red `#FF6369` (error, missing), green `#3DD68C` (done).

**Type ramp** (Fluent 2 Windows ramp, +2 px for Korean; Segoe UI + Malgun Gothic, Semibold = Segoe UI Semibold + Malgun Gothic Bold):

| Name | Size | Weight | Use |
|---|---|---|---|
| Caption | 13 | Regular | shortcuts in tooltips, badge counts |
| Body | 16 | Regular | text |
| Body Strong | 16 | Semibold | names in lists, buttons of occasional screens |
| Body Large | 18 | Regular | empty-state sentence |
| Subtitle | 22 | Semibold | section and panel titles |
| Title | 30 | Semibold | page title on occasional screens |

**Spacing** (4 px grid): 4, 8, 12, 16, 24, 32. Control height 32 on occasional screens, 24 in work-screen toolbars. A two-line list row is 52. Panel padding 16 (occasional) or 10 (work). Rounding 4 for controls, 8 for cards, panels and popups.

### 3.2 Rules

The values live in one place, module-debug `src/mealkit/debug/ImGuiLayer.cpp` `applyStyle`. A tool that doesn't link the engine copies that function, names its source in a comment, and gets updated whenever the original changes.

- **Neutral dark layers**: a gutter (gray 1, also the window title bar color through DWM) behind panels (gray 2). Panels are set apart by space and the darker gutter, not by lines.
- **Tabs are flat**: no fill and no underline. The selected tab joins its panel. The tab strip and the status bar sit on the gutter.
- **The accent blue is only for selection, focus, active states and the one primary button of an occasional screen** (section 1). No other colored button fills.
- **State colors**: amber `(1.0, 0.8, 0.3)` for "needs an action" (upgrade, not installed), red `(1.0, 0.45, 0.4)` for errors and missing things. Use them for text and icons (the output icon of a failed job is red), never as button fills.
- **Secondary text is gray 11** (`TextDisabled`): paths, dates, authors, hints. Readable, just quieter.
- **Hover is one neutral step up**, never the accent. A selected row is gray 5 with the blue 8 bar on its left.
- **Show what identifies a row, nothing more** (GNOME HIG): icon, name, one summary line. Paths, folders and sizes go to tooltips, the details of the selection, or `More`. Status shows only when something needs attention (an update, a missing folder, a failed build); a healthy state shows nothing.
- **Font**: section 3.1 ramp, before DPI scale.
- Thin 10 px scrollbars, 4 px frame rounding, the padding and spacing of `applyStyle`.

## 4. Behaviour

- **Text** (Toss's writing): short, plain words, no internal terms (`junction`, `manifest`) on screen. Korean sentences use 해요체 (`폴더를 찾을 수 없어요`), buttons and menu items a verb noun (`열기`, `업데이트`). Say the result (`모듈 2개 업데이트`) and, for errors, what to do next.
- **Every visible string goes through `tr()`** and has its Korean line; sentences with numbers are one `tr` string with the number in it, never English pieces joined together.

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
- **Title bar**: dark, in the gutter color gray 1 `#111113` through DWM (`DWMWA_USE_IMMERSIVE_DARK_MODE`, `DWMWA_CAPTION_COLOR`; Manager `applyEditorCaption`), never the light system bar.

## 6. Bringing a tool to this look

A tool is on the suite look only when every line holds; until then it is on the list in section 7.

1. `applyStyle` copied as `applyEditorStyle`, with a comment naming module-debug `ImGuiLayer.cpp` as its source. Differences are allowed only where the tool's layout needs them, each with a comment (Manager: no window border, `WindowBg` is the gutter and content sits on panel-colored children).
2. Colors, sizes and spacing from the tokens of section 3.1. Fonts: the ramp of section 3.1 (regular and semibold) + the icon font merged (section 2), all at the DPI scale.
3. Layout: content on panels over the gutter, tab strip and status bar on the gutter; no full-width ImGui window title bars inside the OS window.
4. Buttons: icon buttons from (glyph, name) on work screens, icon + label and one primary button on occasional screens (section 1), glyphs from the table in section 2.
5. Text: every string through `tr()` with its Korean line, written per section 4.
6. Title bar and app icon per section 5.
7. A screenshot of the tool beside the editor at the same DPI shows the same colors and spacing.

When `applyStyle` changes, the same change goes to every copy listed in section 7 in the same piece of work. The move to the 3.1 tokens (2026-11-13) is the exception: it goes app by app, tracked in the Tokens column.

## 7. Where each app stands (2026-11-13)

| App | Theme | Tokens 3.1 | Icon font, icon buttons | Korean text | Title bar | App icon |
|---|---|---|---|---|---|---|
| MealKit Editor | `applyStyle` (source) | colors, spacing | yes | yes | DWM | yes (`make_editor_icon.py`) |
| MealKit Manager | copy in `src/main.cpp` | colors, spacing, ramp | yes, icon + label | yes | DWM | yes |
| MkAudio | copy in `tool/main.cpp` | no | yes (panel buttons; panel tabs without icons yet) | few lines | DWM | yes |
| MkUI Designer | copy in `tool/main.cpp` | no | yes (panel buttons; panel tabs without icons yet) | few lines | DWM | yes |
| MkRender | copy in `tool/main.cpp` | no | yes (panel buttons; panel tabs without icons yet) | few lines | DWM | yes |
| MealKit QA | copy in `tool/main.cpp` (one window: no gutter and panels yet, checklist 3) | no | yes | partly | DWM | yes |

The work left is ROADMAP "Design refresh for the rest of the suite". Update this table when a tool moves.

# Screen flow

The screens around a match: title → menu → solo, create a room, join (LAN rooms or an address) or a Steam room → lobby (names, ready, one lobby choice, chat) → loading → match (chat, pause) → results → back to the lobby. On a disconnect or a refused connection, a notice explains why and returns to the menu.

- Version 0.1.0, engine 0.9, requires the engine modules `game`, `ui`, `loc` 0.2, `settings`, `input`, `session`
- Code: `app/Flow.h` (`screens::Flow`), view models `app/Screens.h`, the chat line `view/Chat.h` (for your HUD view model)
- Screens: `modules/screens/ui/{title,menu,join,lobby,loading,pause,results,message}.ui`, text `modules/screens/strings/<lang>.txt`

Until engine 0.9.8 new projects got a copy of this code in `source/app/Flow.*` and `Screens.h` and the screens in `assets/ui/`. A project with that copy can keep it, or delete it and add this module (the calls are the same apart from the save and chat arguments below).

## Adding

```
tools\module.ps1 add -Project <your project> screens
```

The project also needs these input actions in `assets/input.bindings`: `ui.confirm`, `ui.back`, `menu.pause`, `chat.open` (and `menu.settings` for the settings screen), and the engine's `assets/ui/settings.ui` and `theme.ui` as in a new project.

Then call it from your `GameInstance` (`source/app/GameSession.h`):

```cpp
#include "screens/app/Flow.h"

class GameSession final : public mealkit::game::GameInstance {
    bool menus() const override { return true; } // the game draws title/menu/lobby/results
    ...
    screens::Flow m_flow;
};
```

Every frame, at the start of `update`, tell it about the match and give it the save fields and the HUD chat fields:

```cpp
// source/app/GameSession.cpp
screens::FlowGame flow;
flow.playerColors = look.playerColors;
if (started()) {
    const WorldState& s = sim().world().state;
    flow.inMatch = true;
    flow.results = s.flow.phase == matchflow::FlowPhase::Results; // your own "match over" works too
    flow.playerCount = s.playerCount;
    for (u32 i = 0; i < s.playerCount; ++i) flow.score[i] = s.slots[i].score;
}
m_flow.update(ctx, flow, screens::saveFields(m_save), screens::hudChat(m_hudVm, m_hud), dt);
if (m_flow.takeSaveChanged()) mealkit::game::writeSave(m_save, ctx.userFolder, "progress", kSaveVersion);
```

- `saveFields(save)` points at `save.playerName` (`std::string`) and `save.lobbyChoice` (`u32`). Pass `screens::FlowSave{}` to save nothing.
- `hudChat(vm, screen)` points at a HUD view model with `MEALKIT_LIST(screens::ChatLineVm, chat, 5, ...)`, `chatOpen`, `chatDraft` and a text input named `chatInput` in its `.ui`. Pass `screens::FlowChat{}` for no chat during matches (the lobby still has chat).
- `blocksInput()` is true while a menu, pause, chat or results is open: send no game input and keep the camera still. `pausesSimulation()` is true while a solo game is paused.
- `Flow::playerName(session, slot)` is the lobby name of a player, for name tags and score lists.

### Lobby choice

`FlowGame::choices` lists string keys of what a player can pick in the lobby (a character, a team, a car), and `memberChoice` reads another player's pick from their loadout. The module's `lobby.ui` names three options `lobby.option1..3`: give them your names in your `assets/strings/<lang>.txt` (`lobby.option1 = Knight`), or copy `lobby.ui` and change its `select` line for more options. The pick is saved in `lobbyChoice`; put it in the match with `game::setLoadout` in `GameInstance::lobby`.

## Changing the screens

The flow opens `assets/ui/<name>.ui` when your project has one, else the module's. To restyle a screen, copy it from `modules/screens/ui/` to `assets/ui/` and edit your copy: the module stays unmodified, so `module.ps1 update` keeps working. Text works the same way: a line in your `assets/strings/<lang>.txt` replaces the module's line with the same key.

## How it works

Only the engine's `ctx.match` (`game::MatchService`) opens and closes sessions; the flow calls `playOffline`, `host`, `hostOnline`, `join`, `start`, `returnToLobby` and `leave`. Which screen is shown follows `ctx.match.phase()` and `FlowGame::inMatch`. The results screen opens when the game sets `results`, which should come from simulation state so every peer opens it on the same tick; the host sends everyone back to the lobby after `resultsSeconds`.

## Version history

- 0.1.0: first version, from the engine 0.9.7 sample's `app/Flow.*` and screens: save and HUD chat passed as `FlowSave`/`FlowChat`, screens overridable from `assets/ui/`, lobby options `lobby.option1..3`, the title no longer skips on `jump`

#pragma once

#include "mealkit/core/Reflect.h"

#include <string>

// Chat line view model, in view/ so the game's HUD view model (view/Hud.h) can hold a list of them without including app/
namespace screens {

// One chat line: the lobby's chat list, and the game's HUD view model when it shows chat during a match (screens::hudChat)
MEALKIT_STRUCT(ChatLineVm) {
    MEALKIT_FIELD(std::string, name, {}, "Sender");
    MEALKIT_FIELD(std::string, text, {}, "Text");
};

} // namespace screens

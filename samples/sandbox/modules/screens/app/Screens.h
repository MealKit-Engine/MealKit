#pragma once

#include "mealkit/core/Reflect.h"
#include "mealkit/session/Session.h"
#include "screens/view/Chat.h"

#include <string>

// View models of the screen flow (title → menu → join → lobby → loading → match (pause, results) → lobby).
// The module's .ui files (modules/screens/ui/) bind by field name; which screen to open and button handling are in app/Flow.cpp.
// Screens only read these values; only input widgets (name, address, chat, ready, choice) write their own fields.
namespace screens {

MEALKIT_VIEWMODEL(TitleVm) {
    MEALKIT_FIELD(std::string, version, {}, "Game and engine version");
};

MEALKIT_VIEWMODEL(MenuVm) {
    MEALKIT_FIELD(std::string, playerName, {}, "My name (text input, shown in the lobby)");
    MEALKIT_FIELD(bool, onlineAvailable, false, "An online (Steam) room can be made");
    MEALKIT_FIELD(std::string, error, {}, "Why the last action failed");
};

MEALKIT_STRUCT(RoomRowVm) {
    MEALKIT_FIELD(std::string, hostName, {}, "Host name");
    MEALKIT_FIELD(std::string, players, {}, "Players (2/4)");
    MEALKIT_FIELD(std::string, address, {}, "Address");
    MEALKIT_FIELD(bool, compatible, true, "Same version (else it can't be joined)");
    MEALKIT_FIELD(std::string, note, {}, "Why it can't be joined (other version, other data)");
};

MEALKIT_VIEWMODEL(JoinVm) {
    MEALKIT_FIELD(std::string, address, {}, "Address (text input)");
    MEALKIT_LIST(RoomRowVm, rooms, 12, "Rooms on the same network");
    MEALKIT_FIELD(bool, noRooms, true, "No rooms found");
    MEALKIT_FIELD(std::string, error, {}, "Why the last action failed");
};

MEALKIT_STRUCT(LobbyRowVm) {
    MEALKIT_FIELD(std::string, name, {}, "Name");
    MEALKIT_FIELD(std::string, choice, {}, "What they picked in the lobby (empty = none)");
    MEALKIT_FIELD(bool, ready, false, "Ready");
    MEALKIT_FIELD(bool, host, false, "Host");
    MEALKIT_FIELD(bool, you, false, "Me");
    MEALKIT_FIELD(mealkit::Color, color, {}, "Player color");
    MEALKIT_FIELD(std::string, ping, {}, "Round trip to the host (\"45 ms\", empty for the host and solo)");
    MEALKIT_FIELD(std::string, input, {}, "Input device (keyboard/mouse, gamepad, touch; empty in solo)");
};

MEALKIT_VIEWMODEL(LobbyVm) {
    MEALKIT_FIELD(std::string, title, {}, "Room name (solo / address / Steam)");
    MEALKIT_FIELD(bool, connecting, false, "Still connecting");
    MEALKIT_FIELD(bool, isHost, false, "I'm the host");
    MEALKIT_FIELD(bool, offline, false, "Solo");
    MEALKIT_FIELD(bool, online, false, "Steam lobby (invite friends)");
    MEALKIT_FIELD(bool, ready, false, "My ready (toggle)");
    MEALKIT_FIELD(bool, hasChoice, false, "There is something to pick in the lobby (FlowGame::choices)");
    MEALKIT_FIELD(mealkit::u32, choice, 0, "What I picked (select, FlowGame::choices index)");
    MEALKIT_FIELD(bool, canStart, false, "Can start (everyone ready)");
    MEALKIT_FIELD(bool, waiting, false, "Waiting for the host to start");
    MEALKIT_FIELD(bool, needReady, false, "Host: waiting for everyone to be ready");
    MEALKIT_FIELD(std::string, slowWarning, {}, "Players whose ping is over the input delay cap (the match will stutter)");
    MEALKIT_FIELD(std::string, version, {}, "Game version (v0.1.0 (commit))");
    MEALKIT_LIST(LobbyRowVm, players, mealkit::session::kMaxPlayers, "Players");
    MEALKIT_LIST(ChatLineVm, chat, 8, "Chat (recent)");
    MEALKIT_FIELD(std::string, chatDraft, {}, "Chat to send (text input)");
};

MEALKIT_VIEWMODEL(PauseVm) {
    MEALKIT_FIELD(bool, offline, false, "Solo (the game stops)");
    MEALKIT_FIELD(bool, isHost, false, "Host (can go back to the lobby)");
};

MEALKIT_STRUCT(ResultRowVm) {
    MEALKIT_FIELD(mealkit::u32, rank, 1, "Rank");
    MEALKIT_FIELD(std::string, name, {}, "Name");
    MEALKIT_FIELD(mealkit::u32, score, 0, "Score (the game's number, FlowGame::score)");
    MEALKIT_FIELD(bool, you, false, "Me");
    MEALKIT_FIELD(bool, winner, false, "First place");
    MEALKIT_FIELD(mealkit::Color, color, {}, "Player color");
};

MEALKIT_VIEWMODEL(ResultsVm) {
    MEALKIT_LIST(ResultRowVm, rows, mealkit::session::kMaxPlayers, "Ranking");
    MEALKIT_FIELD(bool, isHost, false, "Host (back to lobby button)");
    MEALKIT_FIELD(mealkit::u32, secondsLeft, 0, "Until everyone goes back to the lobby (host)");
};

MEALKIT_VIEWMODEL(MessageVm) {
    MEALKIT_FIELD(std::string, title, {}, "Title (disconnected / can't join)");
    MEALKIT_FIELD(std::string, text, {}, "Notice (why it disconnected)");
};

} // namespace screens

// Steamworks SDK가 없는 빌드용 빈 구현. third_party/steamworks/sdk 에 SDK를 풀면 Steam.cpp가 대신 빌드된다.
#include "aster/platform/Steam.h"

#include "aster/core/Log.h"

#include <cstdlib>
#include <cstring>

namespace aster::steam {

bool compiledIn() { return false; }
bool init() {
    ASTER_WARN("Steam: this build has no Steamworks SDK (see README: Steamworks)");
    return false;
}
void shutdown() {}
void runCallbacks() {}
bool available() { return false; }
u64 localSteamId() { return 0; }
std::string personaName() { return {}; }
std::unique_ptr<net::Transport> createTransport() { return nullptr; }

struct Lobby::Impl {};
Lobby::Lobby() : m(nullptr) {}
Lobby::~Lobby() = default;
void Lobby::create(u32) {}
void Lobby::join(u64) {}
void Lobby::leave() {}
void Lobby::inviteFriends() {}
LobbyState Lobby::state() const { return LobbyState::Failed; }
bool Lobby::isOwner() const { return false; }
u64 Lobby::lobbyId() const { return 0; }
u64 Lobby::ownerSteamId() const { return 0; }
u32 Lobby::memberCount() const { return 0; }
u64 Lobby::takeJoinRequest() { return 0; }

u64 Lobby::parseLaunchLobby(int argc, char** argv) {
    for (int i = 1; i + 1 < argc; ++i)
        if (std::strcmp(argv[i], "+connect_lobby") == 0) return std::strtoull(argv[i + 1], nullptr, 10);
    return 0;
}

} // namespace aster::steam

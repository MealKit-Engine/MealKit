#pragma once

#include "aster/core/Types.h"
#include "aster/net/Transport.h"

#include <memory>
#include <string>

// Steamworks 연동: 초기화, 친구 전용 로비, 친구 초대, Steam 릴레이 전송.
//
// SDK가 third_party/steamworks/sdk 에 있을 때만 실제로 동작한다 (CMake가 자동 감지, ASTER_STEAM=1).
// SDK가 없으면 모든 함수가 "사용 불가"를 돌려주는 빈 구현이 들어가므로, 게임 코드는 #if 없이 쓸 수 있다.
//
// 개발 중에는 앱 ID 480(Valve의 테스트 앱 Spacewar)을 쓴다. 실행 파일 옆의 steam_appid.txt에 적혀 있다.
namespace aster::steam {

bool compiledIn();            // SDK가 들어간 빌드인가
bool init();                  // Steam 클라이언트가 실행 중이어야 한다
void shutdown();
void runCallbacks();          // 매 프레임 호출
bool available();             // init 성공 여부
u64 localSteamId();
std::string personaName();    // 내 Steam 닉네임

// ISteamNetworkingMessages 기반 전송. 주소는 Address::steamId를 쓴다. NAT/방화벽은 Steam 릴레이가 처리한다.
std::unique_ptr<net::Transport> createTransport();

enum class LobbyState : u8 { None, Creating, Joining, InLobby, Failed };

// 친구 전용 로비. 방장이 락스텝 호스트가 되고, 참가자는 방장의 Steam ID로 접속한다.
class Lobby {
public:
    Lobby();
    ~Lobby();
    Lobby(const Lobby&) = delete;
    Lobby& operator=(const Lobby&) = delete;

    void create(u32 maxMembers);
    void join(u64 lobbyId);
    void leave();
    void inviteFriends(); // Steam 오버레이의 초대 창

    LobbyState state() const;
    bool isOwner() const;
    u64 lobbyId() const;
    u64 ownerSteamId() const;
    u32 memberCount() const;

    // 친구 초대를 수락했거나(+connect_lobby 실행 인자 포함) 참가 요청이 들어온 로비. 없으면 0.
    u64 takeJoinRequest();
    static u64 parseLaunchLobby(int argc, char** argv); // "+connect_lobby <id>"

private:
    struct Impl;
    Impl* m;
};

} // namespace aster::steam

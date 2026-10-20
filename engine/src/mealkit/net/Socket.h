#pragma once

#include "mealkit/core/Types.h"

#include <string>

namespace mealkit::net {

inline constexpr u32 kBroadcastIp = 0xFFFFFFFFu; // 255.255.255.255 (같은 네트워크 전체)
inline constexpr u32 kLoopbackIp = 0x7F000001u;  // 127.0.0.1

struct Address {
    u32 ip = 0; // 호스트 바이트 순서 IPv4
    u16 port = 0;
    u64 steamId = 0; // Steam 전송일 때 상대의 Steam ID (그때 ip/port는 0)

    bool operator==(const Address&) const = default;
    std::string toString() const;
};

// "host", "host:port", "1.2.3.4:27015" 형식. 포트가 없으면 defaultPort.
bool resolveAddress(const char* text, u16 defaultPort, Address& out);

bool startup(); // WSAStartup (참조 카운트)
void cleanup();

// 논블로킹 UDP 소켓.
class UdpSocket {
public:
    UdpSocket() = default;
    ~UdpSocket();
    UdpSocket(const UdpSocket&) = delete;
    UdpSocket& operator=(const UdpSocket&) = delete;

    bool open(u16 port, bool loopbackOnly = false); // port 0 = 임의 포트, loopbackOnly = 127.0.0.1에만 바인딩
    // 같은 포트를 여러 소켓이 같이 듣고(같은 PC의 여러 게임), 브로드캐스트를 받는다 (LAN 방 찾기)
    bool openShared(u16 port);
    bool enableBroadcast(); // kBroadcastIp로 보낼 수 있게
    void close();
    bool isOpen() const;
    bool sendTo(const Address& to, const void* data, u32 size);
    i32 receiveFrom(Address& from, void* buffer, u32 capacity); // 받을 패킷이 없으면 -1
    u16 localPort() const;

private:
    u64 m_socket = ~0ull;
};

// 논블로킹 TCP 연결 (에디터의 localhost 가져오기 서버, mealkit/editor/ImportServer.h). 받은 연결만 쓴다.
class TcpStream {
public:
    TcpStream() = default;
    ~TcpStream();
    TcpStream(TcpStream&& other) noexcept;
    TcpStream& operator=(TcpStream&& other) noexcept;
    TcpStream(const TcpStream&) = delete;
    TcpStream& operator=(const TcpStream&) = delete;

    bool isOpen() const;
    void close();
    i32 receive(void* buffer, u32 capacity); // 받은 바이트, 아직 없으면 0, 끊겼거나 오류면 -1
    i32 send(const void* data, u32 size);    // 보낸 바이트 (일부일 수 있다), 지금 못 보내면 0, 오류면 -1

private:
    friend class TcpListener;
    u64 m_socket = ~0ull;
};

class TcpListener {
public:
    TcpListener() = default;
    ~TcpListener();
    TcpListener(const TcpListener&) = delete;
    TcpListener& operator=(const TcpListener&) = delete;

    bool open(u16 port, bool loopbackOnly); // port 0 = 임의 포트
    void close();
    bool isOpen() const;
    bool accept(TcpStream& out); // 기다리는 연결이 있으면 논블로킹 연결로
    u16 localPort() const;

private:
    u64 m_socket = ~0ull;
};

} // namespace mealkit::net

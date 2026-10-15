#pragma once

#include "mealkit/core/Types.h"

#include <string>

namespace mealkit::net {

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
    void close();
    bool isOpen() const;
    bool sendTo(const Address& to, const void* data, u32 size);
    i32 receiveFrom(Address& from, void* buffer, u32 capacity); // 받을 패킷이 없으면 -1
    u16 localPort() const;

private:
    u64 m_socket = ~0ull;
};

} // namespace mealkit::net

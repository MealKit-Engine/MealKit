#pragma once

#include "mealkit/core/Types.h"

#include <string>

namespace mealkit::net {

inline constexpr u32 kBroadcastIp = 0xFFFFFFFFu; // 255.255.255.255 (the whole network)
inline constexpr u32 kLoopbackIp = 0x7F000001u;  // 127.0.0.1
inline constexpr u32 kLoopbackBroadcastIp = 0x7FFFFFFFu; // 127.255.255.255 (every socket on the port on this PC)

struct Address {
    u32 ip = 0; // IPv4 in host byte order
    u16 port = 0;
    u64 userId = 0; // peer user id for online backend transports (Steam ID, a hash of the Product User ID for EOS). ip/port are 0 then

    bool operator==(const Address&) const = default;
    std::string toString() const;
};

// "host", "host:port", "1.2.3.4:27015" forms. defaultPort if there's no port.
bool resolveAddress(const char* text, u16 defaultPort, Address& out);

bool startup(); // WSAStartup (reference counted)
void cleanup();

// Non-blocking UDP socket.
class UdpSocket {
public:
    UdpSocket() = default;
    ~UdpSocket();
    UdpSocket(const UdpSocket&) = delete;
    UdpSocket& operator=(const UdpSocket&) = delete;

    bool open(u16 port, bool loopbackOnly = false); // port 0 = any port, loopbackOnly = bind only to 127.0.0.1
    // Multiple sockets listen on the same port (multiple games on the same PC) and receive broadcasts (LAN room search)
    bool openShared(u16 port);
    bool enableBroadcast(); // allows sending to kBroadcastIp
    void close();
    bool isOpen() const;
    bool sendTo(const Address& to, const void* data, u32 size);
    i32 receiveFrom(Address& from, void* buffer, u32 capacity); // -1 if there's no packet to receive
    u16 localPort() const;

private:
    u64 m_socket = ~0ull;
};

// Non-blocking TCP connection (the editor's localhost import server, mealkit/editor/ImportServer.h). Only accepted connections are used.
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
    i32 receive(void* buffer, u32 capacity); // bytes received, 0 if none yet, -1 if disconnected or error
    i32 send(const void* data, u32 size);    // bytes sent (may be partial), 0 if it can't send now, -1 on error

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

    bool open(u16 port, bool loopbackOnly); // port 0 = any port
    void close();
    bool isOpen() const;
    bool accept(TcpStream& out); // if a connection is waiting, as a non-blocking connection
    u16 localPort() const;

private:
    u64 m_socket = ~0ull;
};

} // namespace mealkit::net

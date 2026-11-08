#include <winsock2.h>
#include <ws2tcpip.h>
#include <mstcpip.h>

#include "mealkit/platform/Socket.h"

#include "mealkit/core/Log.h"

#include <cstdlib>
#include <cstring>

#ifndef SIO_UDP_CONNRESET
#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)
#endif

namespace mealkit::net {
namespace {
int g_startupCount = 0;
}

bool resolveAddress(const char* text, u16 defaultPort, Address& out) {
    std::string host = text;
    u16 port = defaultPort;
    const size_t colon = host.rfind(':');
    if (colon != std::string::npos) {
        port = static_cast<u16>(std::strtoul(host.c_str() + colon + 1, nullptr, 10));
        host.resize(colon);
    }
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    addrinfo* result = nullptr;
    if (getaddrinfo(host.c_str(), nullptr, &hints, &result) != 0 || !result) return false;
    const auto* sin = reinterpret_cast<const sockaddr_in*>(result->ai_addr);
    out.ip = ntohl(sin->sin_addr.s_addr);
    out.port = port;
    freeaddrinfo(result);
    return true;
}

bool startup() {
    if (g_startupCount++ > 0) return true;
    WSADATA data;
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        --g_startupCount;
        MEALKIT_ERROR("WSAStartup failed");
        return false;
    }
    return true;
}

void cleanup() {
    if (g_startupCount > 0 && --g_startupCount == 0) WSACleanup();
}

UdpSocket::~UdpSocket() { close(); }

bool UdpSocket::open(u16 port, bool loopbackOnly) {
    close();
    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) {
        MEALKIT_ERROR("socket() failed (%d)", WSAGetLastError());
        return false;
    }
    u_long nonBlocking = 1;
    ioctlsocket(s, FIONBIO, &nonBlocking);

    // Windows UDP fails the next recvfrom with WSAECONNRESET when the peer's port is closed. Turn this behavior off.
    BOOL reportReset = FALSE;
    DWORD bytes = 0;
    WSAIoctl(s, SIO_UDP_CONNRESET, &reportReset, sizeof(reportReset), nullptr, 0, &bytes, nullptr, nullptr);

    int bufferSize = 1 << 20;
    setsockopt(s, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&bufferSize), sizeof(bufferSize));
    setsockopt(s, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<const char*>(&bufferSize), sizeof(bufferSize));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(loopbackOnly ? INADDR_LOOPBACK : INADDR_ANY);
    addr.sin_port = htons(port);
    if (bind(s, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        MEALKIT_ERROR("bind(port %u) failed (%d) - is the port already in use?", port, WSAGetLastError());
        closesocket(s);
        return false;
    }
    m_socket = static_cast<u64>(s);
    return true;
}

bool UdpSocket::openShared(u16 port) {
    close();
    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) return false;
    u_long nonBlocking = 1;
    ioctlsocket(s, FIONBIO, &nonBlocking);
    BOOL reuse = TRUE; // broadcasts are received by all sockets bound to the same port
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));
    BOOL reportReset = FALSE;
    DWORD bytes = 0;
    WSAIoctl(s, SIO_UDP_CONNRESET, &reportReset, sizeof(reportReset), nullptr, 0, &bytes, nullptr, nullptr);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
    if (bind(s, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        MEALKIT_WARN("bind(shared port %u) failed (%d)", port, WSAGetLastError());
        closesocket(s);
        return false;
    }
    m_socket = static_cast<u64>(s);
    return true;
}

bool UdpSocket::enableBroadcast() {
    if (!isOpen()) return false;
    BOOL on = TRUE;
    return setsockopt(static_cast<SOCKET>(m_socket), SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&on),
                      sizeof(on)) == 0;
}

void UdpSocket::close() {
    if (isOpen()) {
        closesocket(static_cast<SOCKET>(m_socket));
        m_socket = ~0ull;
    }
}

bool UdpSocket::isOpen() const { return m_socket != ~0ull; }

bool UdpSocket::sendTo(const Address& to, const void* data, u32 size) {
    if (!isOpen()) return false;
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(to.ip);
    addr.sin_port = htons(to.port);
    const int sent = sendto(static_cast<SOCKET>(m_socket), static_cast<const char*>(data), static_cast<int>(size), 0,
                            reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
    return sent == static_cast<int>(size);
}

i32 UdpSocket::receiveFrom(Address& from, void* buffer, u32 capacity) {
    if (!isOpen()) return -1;
    sockaddr_in addr{};
    int addrLen = sizeof(addr);
    const int received = recvfrom(static_cast<SOCKET>(m_socket), static_cast<char*>(buffer), static_cast<int>(capacity),
                                  0, reinterpret_cast<sockaddr*>(&addr), &addrLen);
    if (received == SOCKET_ERROR) return -1; // including WSAEWOULDBLOCK: nothing to receive
    from.ip = ntohl(addr.sin_addr.s_addr);
    from.port = ntohs(addr.sin_port);
    return received;
}

u16 UdpSocket::localPort() const {
    if (!isOpen()) return 0;
    sockaddr_in addr{};
    int len = sizeof(addr);
    getsockname(static_cast<SOCKET>(m_socket), reinterpret_cast<sockaddr*>(&addr), &len);
    return ntohs(addr.sin_port);
}

// ---------------------------------------------------------------- TCP

TcpStream::~TcpStream() { close(); }
TcpStream::TcpStream(TcpStream&& other) noexcept : m_socket(other.m_socket) { other.m_socket = ~0ull; }
TcpStream& TcpStream::operator=(TcpStream&& other) noexcept {
    if (this != &other) {
        close();
        m_socket = other.m_socket;
        other.m_socket = ~0ull;
    }
    return *this;
}

bool TcpStream::isOpen() const { return m_socket != ~0ull; }

void TcpStream::close() {
    if (isOpen()) {
        closesocket(static_cast<SOCKET>(m_socket));
        m_socket = ~0ull;
    }
}

i32 TcpStream::receive(void* buffer, u32 capacity) {
    if (!isOpen()) return -1;
    const int got = recv(static_cast<SOCKET>(m_socket), static_cast<char*>(buffer), static_cast<int>(capacity), 0);
    if (got > 0) return got;
    if (got == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK) return 0;
    return -1; // 0 = the peer closed
}

i32 TcpStream::send(const void* data, u32 size) {
    if (!isOpen()) return -1;
    const int sent = ::send(static_cast<SOCKET>(m_socket), static_cast<const char*>(data), static_cast<int>(size), 0);
    if (sent >= 0) return sent;
    return WSAGetLastError() == WSAEWOULDBLOCK ? 0 : -1;
}

TcpListener::~TcpListener() { close(); }

bool TcpListener::open(u16 port, bool loopbackOnly) {
    close();
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) return false;
    u_long nonBlocking = 1;
    ioctlsocket(s, FIONBIO, &nonBlocking);
    BOOL exclusive = TRUE; // so other programs can't hijack the same port
    setsockopt(s, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<const char*>(&exclusive), sizeof(exclusive));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(loopbackOnly ? INADDR_LOOPBACK : INADDR_ANY);
    addr.sin_port = htons(port);
    if (bind(s, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR || listen(s, 16) == SOCKET_ERROR) {
        MEALKIT_WARN("TCP listen on port %u failed (%d)", port, WSAGetLastError());
        closesocket(s);
        return false;
    }
    m_socket = static_cast<u64>(s);
    return true;
}

void TcpListener::close() {
    if (isOpen()) {
        closesocket(static_cast<SOCKET>(m_socket));
        m_socket = ~0ull;
    }
}

bool TcpListener::isOpen() const { return m_socket != ~0ull; }

bool TcpListener::accept(TcpStream& out) {
    if (!isOpen()) return false;
    const SOCKET c = ::accept(static_cast<SOCKET>(m_socket), nullptr, nullptr);
    if (c == INVALID_SOCKET) return false;
    u_long nonBlocking = 1;
    ioctlsocket(c, FIONBIO, &nonBlocking);
    out.close();
    out.m_socket = static_cast<u64>(c);
    return true;
}

u16 TcpListener::localPort() const {
    if (!isOpen()) return 0;
    sockaddr_in addr{};
    int len = sizeof(addr);
    getsockname(static_cast<SOCKET>(m_socket), reinterpret_cast<sockaddr*>(&addr), &len);
    return ntohs(addr.sin_port);
}

} // namespace mealkit::net

#include <winsock2.h>
#include <ws2tcpip.h>
#include <mstcpip.h>

#include "mealkit/net/Socket.h"

#include "mealkit/core/Log.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifndef SIO_UDP_CONNRESET
#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)
#endif

namespace mealkit::net {
namespace {
int g_startupCount = 0;
}

std::string Address::toString() const {
    char text[40];
    if (steamId) {
        snprintf(text, sizeof(text), "steam:%llu", static_cast<unsigned long long>(steamId));
        return text;
    }
    snprintf(text, sizeof(text), "%u.%u.%u.%u:%u", (ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, ip & 0xFF,
             port);
    return text;
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

    // 윈도우 UDP는 상대 포트가 닫혀 있으면 다음 recvfrom이 WSAECONNRESET으로 실패한다. 이 동작을 끈다.
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
    const int received = recvfrom(static_cast<SOCKET>(m_socket), static_cast<char*>(buffer),
                                  static_cast<int>(capacity), 0, reinterpret_cast<sockaddr*>(&addr), &addrLen);
    if (received == SOCKET_ERROR) return -1; // WSAEWOULDBLOCK 포함: 받을 것 없음
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

} // namespace mealkit::net

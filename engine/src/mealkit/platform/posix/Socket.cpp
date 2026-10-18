// UDP 소켓의 POSIX 구현 (Linux·macOS 헤드리스). 패킷 형식·동작은 win32/Socket.cpp와 같다.
#include "mealkit/net/Socket.h"

#include "mealkit/core/Log.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace mealkit::net {

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

bool startup() { return true; } // POSIX는 소켓 라이브러리 초기화가 없다
void cleanup() {}

UdpSocket::~UdpSocket() { close(); }

bool UdpSocket::open(u16 port, bool loopbackOnly) {
    close();
    const int s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s < 0) {
        MEALKIT_ERROR("socket() failed (%d)", errno);
        return false;
    }
    fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK);

    int bufferSize = 1 << 20;
    setsockopt(s, SOL_SOCKET, SO_RCVBUF, &bufferSize, sizeof(bufferSize));
    setsockopt(s, SOL_SOCKET, SO_SNDBUF, &bufferSize, sizeof(bufferSize));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(loopbackOnly ? INADDR_LOOPBACK : INADDR_ANY);
    addr.sin_port = htons(port);
    if (bind(s, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) != 0) {
        MEALKIT_ERROR("bind(port %u) failed (%d) - is the port already in use?", port, errno);
        ::close(s);
        return false;
    }
    m_socket = static_cast<u64>(s);
    return true;
}

void UdpSocket::close() {
    if (isOpen()) {
        ::close(static_cast<int>(m_socket));
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
    const ssize_t sent =
        sendto(static_cast<int>(m_socket), data, size, 0, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
    return sent == static_cast<ssize_t>(size);
}

i32 UdpSocket::receiveFrom(Address& from, void* buffer, u32 capacity) {
    if (!isOpen()) return -1;
    sockaddr_in addr{};
    socklen_t addrLen = sizeof(addr);
    const ssize_t received =
        recvfrom(static_cast<int>(m_socket), buffer, capacity, 0, reinterpret_cast<sockaddr*>(&addr), &addrLen);
    if (received < 0) return -1; // EAGAIN 포함: 받을 것 없음 (ICMP 거부도 무시한다, Windows와 같게)
    from.ip = ntohl(addr.sin_addr.s_addr);
    from.port = ntohs(addr.sin_port);
    return static_cast<i32>(received);
}

u16 UdpSocket::localPort() const {
    if (!isOpen()) return 0;
    sockaddr_in addr{};
    socklen_t len = sizeof(addr);
    getsockname(static_cast<int>(m_socket), reinterpret_cast<sockaddr*>(&addr), &len);
    return ntohs(addr.sin_port);
}

} // namespace mealkit::net

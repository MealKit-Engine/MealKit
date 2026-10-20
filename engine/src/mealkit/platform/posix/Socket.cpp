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

#ifdef MSG_NOSIGNAL
#define MSG_NOSIGNAL_FLAG MSG_NOSIGNAL // 끊긴 연결에 보내도 SIGPIPE로 죽지 않게
#else
#define MSG_NOSIGNAL_FLAG 0 // macOS: 소켓 옵션 SO_NOSIGPIPE
#endif

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

bool UdpSocket::openShared(u16 port) {
    close();
    const int s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s < 0) return false;
    fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK);
    int reuse = 1; // 브로드캐스트는 같은 포트에 묶인 모든 소켓이 받는다
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
#ifdef SO_REUSEPORT
    setsockopt(s, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse));
#endif
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
    if (bind(s, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) != 0) {
        MEALKIT_WARN("bind(shared port %u) failed (%d)", port, errno);
        ::close(s);
        return false;
    }
    m_socket = static_cast<u64>(s);
    return true;
}

bool UdpSocket::enableBroadcast() {
    if (!isOpen()) return false;
    int on = 1;
    return setsockopt(static_cast<int>(m_socket), SOL_SOCKET, SO_BROADCAST, &on, sizeof(on)) == 0;
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
        ::close(static_cast<int>(m_socket));
        m_socket = ~0ull;
    }
}

i32 TcpStream::receive(void* buffer, u32 capacity) {
    if (!isOpen()) return -1;
    const ssize_t got = recv(static_cast<int>(m_socket), static_cast<char*>(buffer), static_cast<int>(capacity), 0);
    if (got > 0) return static_cast<i32>(got);
    if (got < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return 0;
    return -1; // 0 = 상대가 닫았다
}

i32 TcpStream::send(const void* data, u32 size) {
    if (!isOpen()) return -1;
    const ssize_t sent = ::send(static_cast<int>(m_socket), data, size, MSG_NOSIGNAL_FLAG);
    if (sent >= 0) return static_cast<i32>(sent);
    return errno == EAGAIN || errno == EWOULDBLOCK ? 0 : -1;
}

TcpListener::~TcpListener() { close(); }

bool TcpListener::open(u16 port, bool loopbackOnly) {
    close();
    const int s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s < 0) return false;
    fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(loopbackOnly ? INADDR_LOOPBACK : INADDR_ANY);
    addr.sin_port = htons(port);
    if (bind(s, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) != 0 || listen(s, 16) != 0) {
        MEALKIT_WARN("TCP listen on port %u failed (%d)", port, errno);
        ::close(s);
        return false;
    }
    m_socket = static_cast<u64>(s);
    return true;
}

void TcpListener::close() {
    if (isOpen()) {
        ::close(static_cast<int>(m_socket));
        m_socket = ~0ull;
    }
}

bool TcpListener::isOpen() const { return m_socket != ~0ull; }

bool TcpListener::accept(TcpStream& out) {
    if (!isOpen()) return false;
    const int c = ::accept(static_cast<int>(m_socket), nullptr, nullptr);
    if (c < 0) return false;
    fcntl(c, F_SETFL, fcntl(c, F_GETFL, 0) | O_NONBLOCK);
#ifdef SO_NOSIGPIPE
    int noSigpipe = 1;
    setsockopt(c, SOL_SOCKET, SO_NOSIGPIPE, &noSigpipe, sizeof(noSigpipe));
#endif
    out.close();
    out.m_socket = static_cast<u64>(c);
    return true;
}

u16 TcpListener::localPort() const {
    if (!isOpen()) return 0;
    sockaddr_in addr{};
    socklen_t len = sizeof(addr);
    getsockname(static_cast<int>(m_socket), reinterpret_cast<sockaddr*>(&addr), &len);
    return ntohs(addr.sin_port);
}

} // namespace mealkit::net

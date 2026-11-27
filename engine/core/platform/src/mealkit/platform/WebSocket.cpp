// mealkit/platform/WebSocket.h outside a browser (platform/web/WebSocket.cpp there): a small RFC 6455 client over a TCP stream,
// for the multiplayer relay only. ws:// only (no TLS), the handshake's accept key isn't checked.
#include "mealkit/platform/WebSocket.h"

#include "mealkit/core/Log.h"
#include "mealkit/platform/Socket.h"

#include <algorithm>
#include <chrono>
#include <deque>
#include <random>

namespace mealkit::net {
namespace {

constexpr size_t kMaxMessage = 64 * 1024;       // the relay sends at most 4 KB
constexpr size_t kMaxQueued = 4 * 1024 * 1024;  // bytes waiting to go out before the connection counts as stuck
constexpr auto kConnectTimeout = std::chrono::seconds(10);
constexpr u8 kContinuation = 0x0, kText = 0x1, kBinary = 0x2, kClose = 0x8, kPing = 0x9;

std::string base64(const u8* data, size_t size) {
    static constexpr char kDigits[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    for (size_t i = 0; i < size; i += 3) {
        const u32 v = (data[i] << 16) | ((i + 1 < size ? data[i + 1] : 0) << 8) | (i + 2 < size ? data[i + 2] : 0);
        out += kDigits[v >> 18];
        out += kDigits[(v >> 12) & 63];
        out += i + 1 < size ? kDigits[(v >> 6) & 63] : '=';
        out += i + 2 < size ? kDigits[v & 63] : '=';
    }
    return out;
}

} // namespace

struct WebSocket::Impl {
    struct Message {
        bool text;
        std::vector<u8> data;
    };

    TcpStream stream;
    State state = State::Connecting;
    std::chrono::steady_clock::time_point opened = std::chrono::steady_clock::now();
    std::vector<u8> out, in, parts; // bytes to send, bytes received, a fragmented message so far
    u8 partsKind = 0;
    std::deque<Message> messages;
    std::mt19937 random{std::random_device{}()};

    void fail() {
        state = State::Closed;
        stream.close();
    }

    // A client's frames are masked
    void frame(u8 opcode, const void* data, size_t size) {
        out.push_back(static_cast<u8>(0x80 | opcode));
        if (size < 126) {
            out.push_back(static_cast<u8>(0x80 | size));
        } else {
            out.push_back(0x80 | 126);
            out.push_back(static_cast<u8>(size >> 8));
            out.push_back(static_cast<u8>(size));
        }
        const u32 mask = static_cast<u32>(random());
        u8 key[4];
        for (u32 i = 0; i < 4; ++i) out.push_back(key[i] = static_cast<u8>(mask >> (8 * i)));
        const u8* bytes = static_cast<const u8*>(data);
        for (size_t i = 0; i < size; ++i) out.push_back(static_cast<u8>(bytes[i] ^ key[i & 3]));
    }

    void flush() {
        while (!out.empty() && stream.isOpen()) {
            const i32 sent = stream.send(out.data(), static_cast<u32>(out.size()));
            if (sent < 0) fail();
            if (sent <= 0) break;
            out.erase(out.begin(), out.begin() + sent);
        }
        if (out.size() > kMaxQueued) fail();
    }

    // false until the server's answer to the upgrade request is complete
    bool readHandshake() {
        static constexpr char kEnd[] = "\r\n\r\n";
        const auto end = std::search(in.begin(), in.end(), kEnd, kEnd + 4);
        if (end == in.end()) {
            if (in.size() > kMaxMessage) fail();
            return false;
        }
        const std::string head(in.begin(), end);
        in.erase(in.begin(), end + 4);
        if (head.rfind("HTTP/1.1 101", 0) != 0) {
            MEALKIT_WARN("WebSocket: the server refused the connection (%s)", head.substr(0, head.find('\r')).c_str());
            fail();
            return false;
        }
        state = State::Open;
        return true;
    }

    // Takes one whole frame from the received bytes. false when there isn't one
    bool readFrame() {
        if (in.size() < 2) return false;
        const u8 first = in[0], second = in[1];
        size_t header = 2, size = second & 0x7F;
        if (size == 126) header = 4;
        else if (size == 127) header = 10;
        if (in.size() < header) return false;
        if (header > 2) {
            size = 0;
            for (size_t i = 2; i < header; ++i) size = (size << 8) | in[i];
        }
        if ((first & 0x70) || (second & 0x80) || size > kMaxMessage) { // no extensions; a server's frames aren't masked
            fail();
            return false;
        }
        if (in.size() < header + size) return false;
        const bool final = (first & 0x80) != 0;
        const u8 opcode = first & 0x0F;
        std::vector<u8> data(in.begin() + header, in.begin() + header + size);
        in.erase(in.begin(), in.begin() + header + size);
        if (opcode == kClose) {
            frame(kClose, data.data(), std::min<size_t>(data.size(), 2));
            flush();
            fail();
            return false;
        }
        if (opcode == kPing) {
            frame(kPing + 1, data.data(), std::min<size_t>(data.size(), 125)); // pong
        } else if (opcode == kText || opcode == kBinary || opcode == kContinuation) {
            if (opcode != kContinuation) partsKind = opcode;
            parts.insert(parts.end(), data.begin(), data.end());
            if (parts.size() > kMaxMessage) {
                fail();
                return false;
            }
            if (final) {
                messages.push_back({partsKind == kText, std::move(parts)});
                parts.clear();
            }
        }
        return true;
    }

    void pump() {
        if (state == State::Closed) return;
        const i32 connected = stream.connectState();
        if (connected < 0 || (state == State::Connecting && std::chrono::steady_clock::now() - opened > kConnectTimeout)) {
            fail();
            return;
        }
        if (connected == 0) return;
        flush();
        bool ended = false;
        u8 buffer[4096];
        while (stream.isOpen()) {
            const i32 got = stream.receive(buffer, sizeof(buffer));
            if (got < 0) ended = true;
            if (got <= 0) break;
            in.insert(in.end(), buffer, buffer + got);
        }
        if (state == State::Open || (state == State::Connecting && readHandshake()))
            while (state == State::Open && readFrame()) {}
        flush(); // pongs
        if (ended) fail();
    }

    bool send(u8 opcode, const void* data, u32 size) {
        pump();
        if (state != State::Open || size > 0xFFFF) return false;
        frame(opcode, data, size);
        flush();
        return state == State::Open;
    }
};

bool WebSocket::open(const std::string& url) {
    close();
    if (url.rfind("ws://", 0) != 0) {
        MEALKIT_WARN("WebSocket: only ws:// addresses outside a browser, not '%s'", url.c_str());
        return false;
    }
    const size_t slash = url.find('/', 5);
    const std::string server = url.substr(5, slash == std::string::npos ? std::string::npos : slash - 5);
    const std::string path = slash == std::string::npos ? "/" : url.substr(slash);
    if (!startup()) return false;
    Address address;
    auto impl = new Impl();
    if (server.empty() || !resolveAddress(server.c_str(), 80, address) || !impl->stream.connect(address)) {
        MEALKIT_WARN("WebSocket: cannot connect to '%s'", url.c_str());
        delete impl;
        cleanup();
        return false;
    }
    u8 key[16];
    for (u8& k : key) k = static_cast<u8>(impl->random());
    const std::string request = "GET " + path + " HTTP/1.1\r\nHost: " + server +
                                "\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: " +
                                base64(key, sizeof(key)) + "\r\nSec-WebSocket-Version: 13\r\n\r\n";
    impl->out.assign(request.begin(), request.end());
    m = impl;
    return true;
}

void WebSocket::close() {
    if (!m) return;
    if (m->state == State::Open) {
        const u8 normal[2] = {0x03, 0xE8}; // 1000
        m->frame(kClose, normal, sizeof(normal));
        m->flush(); // what was sent before goes out first
    }
    delete m;
    m = nullptr;
    cleanup();
}

WebSocket::State WebSocket::state() const {
    if (!m) return State::Closed;
    m->pump();
    return m->state;
}

bool WebSocket::sendText(const std::string& text) { return m && m->send(kText, text.data(), static_cast<u32>(text.size())); }

bool WebSocket::sendBinary(const void* data, u32 size) { return m && m->send(kBinary, data, size); }

bool WebSocket::receive(std::vector<u8>& data, bool& isText) {
    if (!m) return false;
    if (m->messages.empty()) m->pump();
    if (m->messages.empty()) return false;
    isText = m->messages.front().text;
    data = std::move(m->messages.front().data);
    m->messages.pop_front();
    return true;
}

bool pageIsLocal() { return false; }
void showRoomCode(u64) {}

} // namespace mealkit::net

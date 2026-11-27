#pragma once

#include "mealkit/core/Types.h"

#include <string>
#include <vector>

// A WebSocket client for the multiplayer relay (mealkit/net/Relay.h): in a browser the browser's own (platform/web/WebSocket.cpp),
// elsewhere a small one over TCP that speaks only ws:// (platform/WebSocket.cpp; no TLS library in the engine).
// Nothing blocks: calls make what progress they can and messages are queued until receive() takes them.
namespace mealkit::net {

class WebSocket {
public:
    enum class State : u8 { Connecting, Open, Closed };

    WebSocket() = default;
    ~WebSocket() { close(); }
    WebSocket(const WebSocket&) = delete;
    WebSocket& operator=(const WebSocket&) = delete;

    bool open(const std::string& url); // false for an address that can't be used (https pages allow only wss://, native only ws://)
    void close();                      // messages already sent still go out
    State state() const;
    bool sendText(const std::string& text);
    bool sendBinary(const void* data, u32 size);
    // The oldest waiting message. false if there is none
    bool receive(std::vector<u8>& data, bool& isText);

private:
    struct Impl;
    Impl* m = nullptr;
};

// true if the page itself came from this computer (localhost, 127.0.0.1): where a development relay is looked for.
// false outside a browser
bool pageIsLocal();
// A small box over the page with the room's code; clicking it copies the page address with ?room=<code>. 0 removes it.
// Nothing outside a browser
void showRoomCode(u64 code);

} // namespace mealkit::net

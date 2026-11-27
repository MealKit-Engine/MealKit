#include "mealkit/platform/WebSocket.h"

#include <emscripten/emscripten.h>

namespace mealkit::net {
namespace {

// Module.mealkitSockets: the browser sockets by id, each with the queue of messages not yet taken
EM_JS(int, mealkitSocketOpen, (const char* url), {
    const all = Module.mealkitSockets || (Module.mealkitSockets = { next: 1, byId: new Map() });
    let socket;
    try {
        socket = new WebSocket(UTF8ToString(url));
    } catch (e) {
        console.warn("WebSocket: " + e);
        return 0;
    }
    socket.binaryType = "arraybuffer";
    const entry = { socket, queue: [] };
    socket.onmessage = (event) => {
        const text = typeof event.data === "string";
        entry.queue.push({ text, bytes: text ? new TextEncoder().encode(event.data) : new Uint8Array(event.data) });
    };
    const id = all.next++;
    all.byId.set(id, entry);
    return id;
});

EM_JS(void, mealkitSocketClose, (int id), {
    const entry = Module.mealkitSockets && Module.mealkitSockets.byId.get(id);
    if (!entry) return;
    entry.socket.onmessage = null;
    entry.socket.close(); // data already passed to send() goes out before the close frame
    Module.mealkitSockets.byId.delete(id);
});

// WebSocket.readyState: 0 connecting, 1 open, 2 closing, 3 closed
EM_JS(int, mealkitSocketState, (int id), {
    const entry = Module.mealkitSockets && Module.mealkitSockets.byId.get(id);
    return entry ? entry.socket.readyState : 3;
});

EM_JS(int, mealkitSocketSend, (int id, const void* data, int size, int text), {
    const entry = Module.mealkitSockets && Module.mealkitSockets.byId.get(id);
    if (!entry || entry.socket.readyState !== 1) return 0;
    const bytes = HEAPU8.slice(data, data + size);
    entry.socket.send(text ? new TextDecoder().decode(bytes) : bytes);
    return 1;
});

// Size of the oldest waiting message, -1 if none
EM_JS(int, mealkitSocketPeek, (int id), {
    const entry = Module.mealkitSockets && Module.mealkitSockets.byId.get(id);
    return entry && entry.queue.length ? entry.queue[0].bytes.length : -1;
});

// Copies the oldest message into data (room for the peeked size) and drops it. Returns 1 for a text message
EM_JS(int, mealkitSocketTake, (int id, void* data), {
    const message = Module.mealkitSockets.byId.get(id).queue.shift();
    HEAPU8.set(message.bytes, data);
    return message.text ? 1 : 0;
});

EM_JS(int, mealkitPageIsLocal, (), {
    return typeof location !== "undefined" && ["localhost", "127.0.0.1", "[::1]"].includes(location.hostname) ? 1 : 0;
});

EM_JS(void, mealkitShowRoomCode, (const char* code), {
    const text = UTF8ToString(code);
    let box = document.getElementById("mealkit-room");
    if (!text) {
        if (box) box.remove();
        return;
    }
    if (!box) {
        box = document.createElement("button");
        box.id = "mealkit-room";
        box.type = "button";
        box.style.cssText = "position:fixed;top:8px;right:8px;z-index:10;padding:8px 14px;border:1px solid #666;" +
            "border-radius:6px;background:rgba(0,0,0,0.75);color:#eee;font:16px system-ui,sans-serif;cursor:pointer";
        document.body.appendChild(box);
    }
    const link = new URL(location.href);
    link.searchParams.delete("host");
    link.searchParams.delete("autostart");
    link.searchParams.set("room", text);
    box.textContent = "Room " + text + "  (copy link)";
    box.onclick = () => {
        navigator.clipboard.writeText(link.toString()).then(
            () => { box.textContent = "Room " + text + "  (copied)"; },
            () => { box.textContent = link.toString(); });
        document.getElementById("canvas").focus();
    };
});

} // namespace

struct WebSocket::Impl {
    int id; // in Module.mealkitSockets
};

bool WebSocket::open(const std::string& url) {
    close();
    const int id = mealkitSocketOpen(url.c_str());
    if (id) m = new Impl{id};
    return id != 0;
}

void WebSocket::close() {
    if (!m) return;
    mealkitSocketClose(m->id);
    delete m;
    m = nullptr;
}

WebSocket::State WebSocket::state() const {
    const int ready = m ? mealkitSocketState(m->id) : 3;
    return ready == 0 ? State::Connecting : ready == 1 ? State::Open : State::Closed;
}

bool WebSocket::sendText(const std::string& text) {
    return m && mealkitSocketSend(m->id, text.data(), static_cast<int>(text.size()), 1) != 0;
}

bool WebSocket::sendBinary(const void* data, u32 size) {
    return m && mealkitSocketSend(m->id, data, static_cast<int>(size), 0) != 0;
}

bool WebSocket::receive(std::vector<u8>& data, bool& isText) {
    const int size = m ? mealkitSocketPeek(m->id) : -1;
    if (size < 0) return false;
    data.resize(static_cast<size_t>(size));
    isText = mealkitSocketTake(m->id, data.data()) != 0;
    return true;
}

bool pageIsLocal() { return mealkitPageIsLocal() != 0; }

void showRoomCode(u64 code) { mealkitShowRoomCode(code ? std::to_string(code).c_str() : ""); }

} // namespace mealkit::net

#!/usr/bin/env python3
"""MealKit relay: multiplayer for browser builds (modules/online OnlineRelay.cpp). Python 3.8+, standard library only.

  python3 tools/relay.py                                          # this PC only, ws://127.0.0.1:27020
  python3 tools/relay.py --bind 0.0.0.0 --cert full.pem --key key.pem --plain-port 27021   # public

Browser games on https pages connect with wss:// (--cert/--key). Desktop games have no TLS and connect with ws://: with
--cert, --plain-port opens a second port for them. Both ports share the rooms.
Behind a reverse proxy that does the TLS, run it without --cert. --origin limits which pages may connect.

A game connects over a WebSocket and sends one request as a text message:
  host <game>            open a room
  join <code> <game>     enter it
The relay answers "room <code> <your id> <owner id>" or "error <reason>" (and closes). A binary message is
[peer id, u64 little endian][packet]: the id names the receiver on the way in and the sender on the way out, so the relay
never looks inside the game's packets. The owner (the lockstep host) may pass ownership on with "owner <id>"; when it
leaves, the oldest member gets it. Every member is told with "owner <id>".
"""
import argparse
import asyncio
import base64
import hashlib
import itertools
import secrets
import ssl
import struct

MAX_MESSAGE = 4096             # game packets are at most 1170 bytes (net/Lockstep.cpp kMaxPacket)
MAX_MEMBERS = 16               # the game's own player limit is lower; this leaves room for reconnects
MAX_ROOMS = 1000
MAX_QUEUED = 4 * 1024 * 1024   # a member that stops reading is dropped instead of filling memory
REQUEST_TIMEOUT = 10.0         # handshake and first request
PING_INTERVAL = 20.0
SILENCE_TIMEOUT = 60.0         # browsers answer pings even in a hidden tab

TEXT, BINARY, CLOSE, PING, PONG = 0x1, 0x2, 0x8, 0x9, 0xA

rooms = {}                     # code -> Room
ids = itertools.count(1)


class Room:
    def __init__(self, code, game, owner):
        self.code, self.game, self.owner = code, game, owner
        self.members = {}      # id -> Member, oldest first


class Member:
    def __init__(self, writer):
        self.writer = writer
        self.id = next(ids)

    def send(self, opcode, data):
        if self.writer.is_closing():
            return
        size = len(data)
        if size < 126:
            header = struct.pack("!BB", 0x80 | opcode, size)
        else:
            header = struct.pack("!BBH", 0x80 | opcode, 126, size)
        self.writer.write(header + data)
        if self.writer.transport.get_write_buffer_size() > MAX_QUEUED:
            self.writer.transport.abort()

    def text(self, line):
        self.send(TEXT, line.encode())


async def handshake(reader, writer, origins):
    head = (await reader.readuntil(b"\r\n\r\n")).decode("latin-1").split("\r\n")
    headers = {}
    for line in head[1:]:
        name, _, value = line.partition(":")
        headers[name.strip().lower()] = value.strip()
    key = headers.get("sec-websocket-key")
    if not head[0].startswith("GET ") or not key or headers.get("upgrade", "").lower() != "websocket":
        writer.write(b"HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\nConnection: close\r\n\r\n")
        return False
    # --origin limits web pages; a request without an Origin comes from a desktop game
    if origins and "origin" in headers and headers["origin"] not in origins:
        writer.write(b"HTTP/1.1 403 Forbidden\r\nContent-Length: 0\r\nConnection: close\r\n\r\n")
        return False
    accept = base64.b64encode(hashlib.sha1((key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11").encode()).digest())
    writer.write(b"HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                 b"Sec-WebSocket-Accept: " + accept + b"\r\n\r\n")
    return True


async def read_frame(reader):
    first, second = await reader.readexactly(2)
    size = second & 0x7F
    if first & 0x70 or not second & 0x80:  # no extensions; a client's frames are masked
        raise ValueError("bad frame")
    if size == 126:
        size = struct.unpack("!H", await reader.readexactly(2))[0]
    elif size == 127:
        size = struct.unpack("!Q", await reader.readexactly(8))[0]
    if size > MAX_MESSAGE:
        raise ValueError("message too large")
    mask = await reader.readexactly(4)
    data = await reader.readexactly(size)
    if size:
        pad = (mask * (size // 4 + 1))[:size]
        data = (int.from_bytes(data, "little") ^ int.from_bytes(pad, "little")).to_bytes(size, "little")
    return bool(first & 0x80), first & 0x0F, data


async def read_message(reader, member):
    """The next text or binary message; answers pings on the way. None when the peer closes."""
    kind, parts = None, b""
    while True:
        final, opcode, data = await asyncio.wait_for(read_frame(reader), SILENCE_TIMEOUT)
        if opcode == CLOSE:
            member.send(CLOSE, data[:2])
            return None
        if opcode == PING:
            member.send(PONG, data)
        elif opcode in (TEXT, BINARY, 0):
            if (opcode == 0) == (kind is None):
                raise ValueError("bad fragment")
            kind = kind or opcode
            parts += data
            if len(parts) > MAX_MESSAGE:
                raise ValueError("message too large")
            if final:
                return kind, parts


def enter(member, request):
    """The room for a "host" or "join" request, or the reason there is none."""
    command, _, rest = request.partition(" ")
    if command == "host" and rest:
        if len(rooms) >= MAX_ROOMS:
            return None, "the relay is full"
        code = 0
        while not code or code in rooms:
            code = 100000 + secrets.randbelow(900000)
        room = rooms[code] = Room(code, rest, member.id)
    elif command == "join":
        number, _, game = rest.partition(" ")
        room = rooms.get(int(number)) if number.isdigit() else None
        if not room or room.game != game:
            return None, "no such room"
        if len(room.members) >= MAX_MEMBERS:
            return None, "the room is full"
    else:
        return None, "unknown request"
    room.members[member.id] = member
    member.text("room %d %d %d" % (room.code, member.id, room.owner))
    return room, ""


def set_owner(room, owner):
    room.owner = owner
    for m in room.members.values():
        m.text("owner %d" % owner)


def leave(room, member):
    room.members.pop(member.id, None)
    if not room.members:
        rooms.pop(room.code, None)
    elif room.owner == member.id:
        set_owner(room, next(iter(room.members)))


async def ping(member):
    while True:
        await asyncio.sleep(PING_INTERVAL)
        member.send(PING, b"")


async def serve(reader, writer, origins):
    member = Member(writer)
    room = pinger = None
    try:
        if not await asyncio.wait_for(handshake(reader, writer, origins), REQUEST_TIMEOUT):
            return
        message = await asyncio.wait_for(read_message(reader, member), REQUEST_TIMEOUT)
        if not message or message[0] != TEXT:
            return
        room, reason = enter(member, message[1].decode("utf-8", "replace"))
        if not room:
            member.text("error " + reason)
            member.send(CLOSE, struct.pack("!H", 1000))
            return
        print("room %d: %d joined (%d in room, %d rooms)" % (room.code, member.id, len(room.members), len(rooms)), flush=True)
        pinger = asyncio.ensure_future(ping(member))
        while True:
            message = await read_message(reader, member)
            if not message:
                return
            kind, data = message
            if kind == BINARY and len(data) >= 8:
                target = room.members.get(int.from_bytes(data[:8], "little"))
                if target:
                    target.send(BINARY, member.id.to_bytes(8, "little") + data[8:])
            elif kind == TEXT:
                command, _, value = data.decode("utf-8", "replace").partition(" ")
                if command == "owner" and room.owner == member.id and value.isdigit() and int(value) in room.members:
                    set_owner(room, int(value))
    except (asyncio.IncompleteReadError, asyncio.LimitOverrunError, asyncio.TimeoutError, ConnectionError, ValueError):
        pass
    finally:
        if pinger:
            pinger.cancel()
        if room:
            leave(room, member)
            print("room %d: %d left (%d rooms)" % (room.code, member.id, len(rooms)), flush=True)
        writer.close()


async def main():
    parser = argparse.ArgumentParser(description="MealKit relay server for browser multiplayer")
    parser.add_argument("--bind", default="127.0.0.1", help="address to listen on (0.0.0.0 for everyone)")
    parser.add_argument("--port", type=int, default=27020)
    parser.add_argument("--cert", help="TLS certificate chain (PEM): serve wss://")
    parser.add_argument("--key", help="TLS private key (PEM)")
    parser.add_argument("--plain-port", type=int, help="a second port without TLS, for desktop games (use with --cert)")
    parser.add_argument("--origin", action="append", help="allowed page origin, e.g. https://example.com (repeatable)")
    args = parser.parse_args()
    tls = None
    if args.cert:
        tls = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        tls.load_cert_chain(args.cert, args.key)
    ports = [(args.port, tls)] + ([(args.plain_port, None)] if args.plain_port else [])
    servers = []
    for port, security in ports:
        servers.append(await asyncio.start_server(lambda r, w: serve(r, w, args.origin), args.bind, port, ssl=security, limit=8192))
        print("MealKit relay on %s://%s:%d" % ("wss" if security else "ws", args.bind, port), flush=True)
    await asyncio.gather(*(s.serve_forever() for s in servers))


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        pass

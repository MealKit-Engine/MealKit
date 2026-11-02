"""Writes the sample terrain heightmap assets/terrain/hills.png (16-bit grayscale PNG, 129 x 129) used by assets/scenes/terrain.scene.
Standard library only. The image's top row is the +z edge (asset/Terrain.h)."""
import math, os, struct, zlib

N = 129
root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "assets", "terrain")

def height(x, z):
    u, v = x / (N - 1) * 2 - 1, z / (N - 1) * 2 - 1
    h = 0.25 + 0.18 * math.sin(u * 3.1) * math.cos(v * 2.3) + 0.12 * math.sin(u * 7.0 + v * 5.0) * 0.5
    h += 0.35 * math.exp(-((u - 0.45) ** 2 + (v - 0.4) ** 2) * 9)  # hill
    h -= 0.15 * math.exp(-((u + 0.5) ** 2 + (v + 0.3) ** 2) * 14)  # pond
    h = 0.25 + (h - 0.25) * min(1.0, (u * u + v * v) * 3)  # flat middle for spawns
    return min(max(h, 0.0), 1.0)

def chunk(kind, data):
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)

raw = bytearray()
for row in range(N):
    raw.append(0)
    z = N - 1 - row
    for x in range(N):
        raw += struct.pack(">H", round(height(x, z) * 65535))
png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", N, N, 16, 0, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(bytes(raw), 9)) + chunk(b"IEND", b"")
os.makedirs(root, exist_ok=True)
with open(os.path.join(root, "hills.png"), "wb") as f:
    f.write(png)
print("wrote assets/terrain/hills.png")

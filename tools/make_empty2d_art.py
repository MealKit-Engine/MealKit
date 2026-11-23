"""2D template art generator: draws the pixel art of templates/empty2d with only the standard library (own work, CC0).

    python tools/make_empty2d_art.py   -> templates/empty2d/assets/textures/{hero,ground,platform_4,platform_3}.png

All images are drawn for 16 pixels per meter (the scene's sprite objects use pixelsPerUnit 16):
  hero.png      4 x 2 cells of 14 x 30 px (0.875 x 1.875 m, the character's collision box is 0.8 x 1.8): row 0 = idle (4 frames),
                row 1 = run (4 frames). Drawn light grey so the player color (sprite tint) shows; eyes and outline stay dark
  ground.png    960 x 16 px (60 x 1 m), platform_4.png 64 x 8 px (4 x 0.5 m), platform_3.png 48 x 8 px (3 x 0.5 m)
The sprite clips that name the hero's frames are templates/empty2d/assets/tables/sprite_clips.table.
"""

import os
import struct
import zlib

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "templates", "empty2d", "assets", "textures")


def save_png(path, width, height, pixels):
    """pixels: list of rows, each a list of (r, g, b, a). 8-bit RGBA, straight alpha."""
    raw = b"".join(b"\x00" + bytes(c for px in row for c in px) for row in pixels)

    def chunk(kind, data):
        body = kind + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)

    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(png)


class Canvas:
    def __init__(self, width, height):
        self.w, self.h = width, height
        self.px = [[(0, 0, 0, 0)] * width for _ in range(height)]

    def set(self, x, y, c):
        if 0 <= x < self.w and 0 <= y < self.h:
            self.px[y][x] = c

    def rect(self, x0, y0, x1, y1, c):  # inclusive corners
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                self.set(x, y, c)

    def save(self, name):
        save_png(os.path.join(ROOT, name), self.w, self.h, self.px)


def hash2(x, y):  # fixed pseudo-random speckle (no random module state: the art is the same on every run)
    n = (x * 374761393 + y * 668265263) & 0xFFFFFFFF
    n = ((n ^ (n >> 13)) * 1274126177) & 0xFFFFFFFF
    return (n ^ (n >> 16)) & 0xFF


def shade(c, d):
    return (max(0, min(255, c[0] + d)), max(0, min(255, c[1] + d)), max(0, min(255, c[2] + d)), 255)


def ground():
    cv = Canvas(960, 16)
    dirt, grass = (122, 84, 56, 255), (96, 150, 118, 255)
    for x in range(960):
        for y in range(16):
            if y < 3:
                c = shade(grass, 10 if (x + y) % 5 == 0 else 0)
            elif y == 3:
                c = shade(grass, -45) if hash2(x, 0) % 3 else shade(dirt, 0)  # ragged lower grass edge
            else:
                c = shade(dirt, (hash2(x, y) % 24) - 12)
            cv.set(x, y, c)
    for y in range(4, 16):  # small stones
        for x in range(960):
            if hash2(x // 3, y // 3 + 99) > 245 and y % 3 != 0:
                cv.set(x, y, (92, 62, 44, 255))
    cv.save("ground.png")


def platform(width_px, name):
    cv = Canvas(width_px, 8)
    top, body, edge = (150, 130, 235, 255), (138, 123, 214, 255), (88, 72, 160, 255)
    cv.rect(0, 0, width_px - 1, 1, top)
    cv.rect(0, 2, width_px - 1, 6, body)
    cv.rect(0, 7, width_px - 1, 7, edge)
    cv.rect(0, 0, 0, 7, edge)
    cv.rect(width_px - 1, 0, width_px - 1, 7, edge)
    for x in range(4, width_px - 2, 8):  # rivets
        cv.set(x, 4, edge)
    cv.save(name)


def hero():
    cw, ch = 14, 30
    cv = Canvas(cw * 4, ch * 2)
    outline, body, belly, eye = (30, 30, 38, 255), (225, 225, 232, 255), (200, 200, 210, 255), (20, 20, 28, 255)

    def frame(col, row, bob, legs):
        ox, oy = col * cw, row * ch
        top = 4 + bob  # head top
        # body block (rounded corners by leaving the corner pixels empty)
        for y in range(top, top + 18):
            for x in range(1, 13):
                corner = (y in (top, top + 17)) and x in (1, 12)
                if not corner:
                    cv.set(ox + x, oy + y, outline if x in (1, 12) or y in (top, top + 17) else body)
        cv.rect(ox + 3, oy + top + 11, ox + 10, oy + top + 15, belly)
        # eyes (face the right; the view mirrors the sprite to face left)
        cv.rect(ox + 8, oy + top + 4, ox + 9, oy + top + 7, eye)
        cv.rect(ox + 5, oy + top + 4, ox + 6, oy + top + 7, eye)
        cv.set(ox + 8, oy + top + 4, (255, 255, 255, 255))
        cv.set(ox + 5, oy + top + 4, (255, 255, 255, 255))
        # legs: (left x offset, right x offset, left lift, right lift)
        lx, rx, ll, rl = legs
        base = 28
        for lift, x0 in ((ll, 3 + lx), (rl, 8 + rx)):
            cv.rect(ox + x0, oy + top + 18, ox + x0 + 2, oy + base - lift, outline)
            cv.rect(ox + x0 + 1, oy + top + 18, ox + x0 + 1, oy + base - lift - 1, body)

    idle = [(0, (0, 0, 0, 0)), (0, (0, 0, 0, 0)), (1, (0, 0, 0, 0)), (1, (0, 0, 0, 0))]
    for i, (bob, legs) in enumerate(idle):
        frame(i, 0, 1 if bob else 0, (0, 0, 0, 0))
    run = [(-3, 3, 2, 0), (-1, 1, 0, 0), (3, -3, 0, 2), (1, -1, 0, 0)]
    for i, legs in enumerate(run):
        frame(i, 1, 1 if i % 2 == 0 else 0, legs)
    cv.save("hero.png")


if __name__ == "__main__":
    ground()
    platform(64, "platform_4.png")
    platform(48, "platform_3.png")
    hero()
    print("wrote", os.path.normpath(ROOT))

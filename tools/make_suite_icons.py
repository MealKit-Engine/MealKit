"""App icons for the MealKit suite (needs Pillow and numpy: pip install pillow numpy). docs/TOOL_DESIGN.md section 5.

    python tools/make_suite_icons.py            -> writes each tool's .ico into its sibling repo (..\\<repo>\\...)
    python tools/make_suite_icons.py --out DIR  -> writes all of them (and the editor's) into DIR instead, plus sheet.png (preview)

The editor icon (tools/make_editor_icon.py) uses the same drawing code from here.

One shape for the whole suite: every app is the same kit box (a lid over a body, straight-on) in its own color, with one
large white symbol on the front. The box says MealKit, the color and the symbol say which app.
Windows 11 app icon rules (learn.microsoft.com, Windows apps > Design > Iconography > App icon design): 48x48 grid (U = one
unit), rounded corners, 120 degree gradients lit from the top left, soft layer shadows defined at 48 px and scaled, at most
two metaphors (box + symbol), no letters. Sizes up to 24 px use a small drawing: thinner lid, larger and simpler symbol.
Drawn at 1024 px and scaled down to each size.
"""

import argparse
import os

import numpy as np
from PIL import Image, ImageChops, ImageDraw, ImageFilter

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WORKSPACE = os.path.dirname(ROOT)
SIZES = [16, 20, 24, 32, 40, 48, 64, 128, 256]
SMALL_UP_TO = 24
N = 1024
U = N / 48.0
WHITE = ((255, 255, 255), (226, 232, 242))

# app -> (body light, body dark)
COLORS = {
    "MealKitEditor": ((255, 176, 72), (230, 108, 28)),
    "MealKitManager": ((96, 156, 248), (38, 88, 200)),
    "MkAudio": ((78, 206, 120), (22, 142, 82)),
    "MkUI": ((186, 128, 248), (118, 66, 210)),
    "MkRender": ((44, 200, 214), (10, 126, 160)),
    "MkQA": ((252, 104, 132), (208, 48, 90)),
}


def p(*v):
    # Grid units -> pixels
    return [round(x * U) for x in v]


def r(units):
    return round(units * U)


def tint(c, t):
    return tuple(round(x + (255 - x) * t) for x in c)


def gradient(c1, c2, angle=120):
    # Linear gradient across the whole canvas; 120 degrees runs from the top left to the bottom right
    a = np.deg2rad(angle - 90)
    yy, xx = np.mgrid[0:N, 0:N] / (N - 1)
    t = (xx - 0.5) * np.cos(a) + (yy - 0.5) * np.sin(a)
    t = (t - t.min()) / (t.max() - t.min())
    c1, c2 = np.array(c1, float), np.array(c2, float)
    rgb = c1 + (c2 - c1) * t[..., None]
    return Image.fromarray(np.dstack([rgb, np.full((N, N), 255.0)]).astype(np.uint8))


def shape(draw_fn):
    m = Image.new("L", (N, N), 0)
    draw_fn(ImageDraw.Draw(m))
    return m


class Icon:
    def __init__(self):
        self.img = Image.new("RGBA", (N, N), (0, 0, 0, 0))
        self.below = Image.new("L", (N, N), 0)  # where shadows of the next layers may fall

    def shadow(self, mask, dy, blur, alpha, clip=None):
        s = mask.filter(ImageFilter.GaussianBlur(blur * U))
        moved = Image.new("L", (N, N), 0)  # shift down without wrapping the bottom rows round to the top
        moved.paste(s, (0, round(dy * U)))
        s = moved.point(lambda v: round(v * alpha))
        if clip is not None:
            s = ImageChops.multiply(s, clip)
        layer = Image.new("RGBA", (N, N), (0, 0, 0, 0))
        layer.putalpha(s)
        self.img.alpha_composite(layer)

    def layer(self, mask, c1, c2, shadow="object"):
        if shadow == "object":  # a separate object resting on the surface: a soft shadow and a contact shadow
            self.shadow(mask, 1.0, 1.0, 0.28)
            self.shadow(mask, 0.0, 0.25, 0.18)
        elif shadow == "inner":  # on top of the shape below: a tight shadow kept inside it
            self.shadow(mask, 0.25, 0.4, 0.22, clip=self.below)
        self.img.paste(gradient(c1, c2), (0, 0), mask)


class Box:
    """The shared kit box. Symbols are drawn in grid units around the centre of the visible front (g)."""

    def __init__(self, colors, small):
        self.ic = Icon()
        self.small = small
        self.colors = colors
        c1, c2 = colors
        if small:
            body, lid, self.c, self.s = (5, 12, 43, 44), (3, 6, 45, 15), (24, 29.5), 1.3
        else:
            body, lid, self.c, self.s = (6, 14, 42, 43), (4, 7, 44, 18), (24, 30.5), 1.0
        bodym = shape(lambda d: d.rounded_rectangle(p(*body), radius=r(3), fill=255))
        self.ic.layer(bodym, c1, c2)
        lidm = shape(lambda d: d.rounded_rectangle(p(*lid), radius=r(2.5), fill=255))
        self.ic.shadow(lidm, 0.6, 0.6, 0.30, clip=bodym)  # the lid's shadow falls on the body only
        self.ic.layer(lidm, tint(c1, 0.35), tint(c2, 0.22), shadow=None)
        self.ic.below = bodym  # symbols cast their shadows on the body, never past it

    def g(self, x, y):
        return (self.c[0] + x * self.s, self.c[1] + y * self.s)

    def pts(self, seq):
        return [tuple(p(*self.g(x, y))) for x, y in seq]

    def rect(self, x0, y0, x1, y1, rad):
        a, b = self.g(x0, y0), self.g(x1, y1)
        return lambda d: d.rounded_rectangle(p(a[0], a[1], b[0], b[1]), radius=r(rad * self.s), fill=255)

    def mark(self, draw_fn, c1=WHITE[0], c2=WHITE[1], shadow="inner"):
        self.ic.layer(shape(draw_fn), c1, c2, shadow=shadow)


# ---------------------------------------------------------------- one symbol per app


def editor(b):
    # A solid cube in three whites: the engine's 3D
    top, half, h = -10, 9, 10
    q = half / 2
    t = [(0, top), (half, top + q), (0, top + 2 * q), (-half, top + q)]
    lf = [(-half, top + q), (0, top + 2 * q), (0, top + 2 * q + h), (-half, top + q + h)]
    rt = [(0, top + 2 * q), (half, top + q), (half, top + q + h), (0, top + 2 * q + h)]
    outline = [t[0], t[1], rt[2], rt[3], lf[3], t[3]]
    b.ic.shadow(shape(lambda d: d.polygon(b.pts(outline), fill=255)), 0.5, 0.75, 0.30, clip=b.ic.below)
    for face, c1, c2 in [(lf, (246, 248, 252), (226, 232, 242)), (rt, (208, 216, 232), (184, 194, 214)), (t, (255, 255, 255), (250, 251, 253))]:
        b.mark(lambda d, f=face: d.polygon(b.pts(f), fill=255), c1, c2, shadow=None)


def manager(b):
    # Four tiles: where projects, engines and tools are gathered
    s, gap = (8.5, 2.6) if not b.small else (8, 3)
    o = (s * 2 + gap) / 2
    for i in range(4):
        x, y = -o + (i % 2) * (s + gap), -o + (i // 2) * (s + gap)
        b.mark(b.rect(x, y, x + s, y + s, 2.2))


def audio(b):
    # Waveform
    heights, w, gap = ([9, 18, 12], 4.6, 3.2) if b.small else ([7, 14, 19, 12, 8], 3, 2.2)
    x0 = -(len(heights) * w + (len(heights) - 1) * gap) / 2
    for i, h in enumerate(heights):
        x = x0 + i * (w + gap)
        b.mark(b.rect(x, -h / 2, x + w, h / 2, w / 2))


def ui(b):
    # A window with a side column and content blocks
    b.mark(b.rect(-11.5, -9.5, 11.5, 9.5, 2))
    c1, c2 = b.colors
    accent = (tint(c1, 0.15), tint(c2, 0.05))
    block = ((200, 206, 224), (172, 180, 204))
    b.mark(b.rect(-9, -4.5, -3.5, 7, 1), *accent, shadow=None)
    if b.small:
        b.mark(b.rect(-1.5, -4.5, 9, 7, 1), *block, shadow=None)
        return
    b.mark(b.rect(-1.5, -4.5, 9, 0.6, 1), *block, shadow=None)
    b.mark(b.rect(-1.5, 2.2, 9, 7, 1), *block, shadow=None)
    b.mark(b.rect(-9, -7.6, -6.8, -6.4, 0.6), *accent, shadow=None)


def render(b):
    # A lit sphere: shading is what the renderer makes
    cx, cy = b.g(0, -1)
    rad = 9.8 * b.s
    m = shape(lambda d: d.ellipse(p(cx - rad, cy - rad, cx + rad, cy + rad), fill=255))
    b.ic.shadow(m, 0.6, 0.8, 0.32, clip=b.ic.below)
    yy, xx = np.mgrid[0:N, 0:N].astype(float)
    nx, ny = (xx - cx * U) / (rad * U), (yy - cy * U) / (rad * U)
    nz = np.sqrt(np.clip(1.0 - nx * nx - ny * ny, 0, 1))
    light = np.array([-0.5, -0.62, 0.6])
    light /= np.linalg.norm(light)
    diff = np.clip(nx * light[0] + ny * light[1] + nz * light[2], 0, 1)
    dark, bright = np.array([120, 196, 214.0]), np.array([255, 255, 255.0])
    rgb = dark + (bright - dark) * diff[..., None] ** 0.9
    b.ic.img.alpha_composite(Image.fromarray(np.dstack([np.clip(rgb, 0, 255), np.array(m, float)]).astype(np.uint8)))


def qa(b):
    # A check mark
    pts = [(-8.5, 0.5), (-2.8, 6.2), (9, -6.5)]
    wd = (4.6 if b.small else 4.2) * b.s
    ends = [b.g(*q) for q in (pts[0], pts[2])]
    b.mark(lambda d: (d.line(b.pts(pts), fill=255, width=r(wd), joint="curve"),
                      [d.ellipse(p(x - wd / 2, y - wd / 2, x + wd / 2, y + wd / 2), fill=255) for x, y in ends]))


SYMBOLS = {"MealKitEditor": editor, "MealKitManager": manager, "MkAudio": audio, "MkUI": ui, "MkRender": render, "MkQA": qa}

# Tools written by this script (the editor's .ico and .png are written by make_editor_icon.py)
TOOLS = {
    "MealKitManager": os.path.join("MealKit-Manager", "res", "MealKitManager.ico"),
    "MkAudio": os.path.join("MkAudio", "tool", "res", "MkAudio.ico"),
    "MkUI": os.path.join("MkUI", "tool", "res", "MkUI.ico"),
    "MkRender": os.path.join("MkRender", "tool", "res", "MkRender.ico"),
    "MkQA": os.path.join("MealKit-QA", "tool", "res", "MkQA.ico"),
}


def draw(name, small=False):
    b = Box(COLORS[name], small)
    SYMBOLS[name](b)
    return b.ic.img


class AppIcon:
    """Both drawings of one app; at(size) picks the right one."""

    def __init__(self, name):
        self.big, self.small = draw(name), draw(name, small=True)

    def at(self, size):
        return (self.small if size <= SMALL_UP_TO else self.big).resize((size, size), Image.LANCZOS)

    def save_ico(self, path):
        os.makedirs(os.path.dirname(path), exist_ok=True)
        images = [self.at(sz) for sz in SIZES]
        images[-1].save(path, format="ICO", sizes=[(sz, sz) for sz in SIZES], append_images=images[:-1])


def sheet(icons, path):
    # Preview: every app on the Windows light and dark surfaces at 128, 64, 48, 32, 24 and 16 px
    show, cell = [128, 64, 48, 32, 24, 16], 150
    w, h = cell * len(show) * 2, cell * len(icons)
    img = Image.new("RGBA", (w, h))
    ImageDraw.Draw(img).rectangle([0, 0, w // 2, h], fill=(243, 243, 243, 255))
    ImageDraw.Draw(img).rectangle([w // 2, 0, w, h], fill=(32, 32, 32, 255))
    for row, icon in enumerate(icons.values()):
        for half in range(2):
            for i, s in enumerate(show):
                x = half * w // 2 + i * cell + (cell - s) // 2
                img.alpha_composite(icon.at(s), (x, row * cell + (cell - s) // 2))
    img.save(path)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", help="write every icon into this folder instead of the repos")
    args = parser.parse_args()
    if args.out:
        icons = {name: AppIcon(name) for name in SYMBOLS}
        for name, icon in icons.items():
            icon.save_ico(os.path.join(args.out, name + ".ico"))
            icon.at(256).save(os.path.join(args.out, name + ".png"))
        sheet(icons, os.path.join(args.out, "sheet.png"))
        print("wrote", args.out)
        return
    for name, rel in TOOLS.items():
        out = os.path.join(WORKSPACE, rel)
        AppIcon(name).save_ico(out)
        print("wrote", out)


if __name__ == "__main__":
    main()

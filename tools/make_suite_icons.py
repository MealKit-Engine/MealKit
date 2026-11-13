"""App icons for the MealKit suite (needs Pillow and numpy: pip install pillow numpy). docs/TOOL_DESIGN.md section 5.

    python tools/make_suite_icons.py            -> writes each tool's .ico into its sibling repo (..\\<repo>\\...)
    python tools/make_suite_icons.py --out DIR  -> writes all of them (and the editor's) into DIR instead, plus sheet.png (preview)

The editor icon (tools/make_editor_icon.py) uses the same drawing code from here.

Drawn after the Windows 11 app icon guidance (learn.microsoft.com, Windows apps > Design > Iconography > App icon design):
- no shared plate: each icon is its own silhouette, drawn straight-on on a 48x48 grid (U = one grid unit);
  only the 3D apps (editor cube, MkRender sphere) show depth
- exterior corners 2 units, interior 1 unit; at most two metaphors per icon
- subtle 120 degree gradients, lighter at the top left (ambient light), one hue lane per shape
- layers separated by soft drop shadows defined at 48 px and scaled with the icon
Drawn at 1024 px and scaled down to each size.
"""

import argparse
import os

import numpy as np
from PIL import Image, ImageChops, ImageDraw, ImageFilter

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WORKSPACE = os.path.dirname(ROOT)
SIZES = [16, 20, 24, 32, 40, 48, 64, 128, 256]
N = 1024
U = N / 48.0


def p(*v):
    # Grid units -> pixels
    return [round(x * U) for x in v]


def r(units):
    return round(units * U)


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
        self.below = Image.new("L", (N, N), 0)  # union of the shapes drawn so far, for shadows kept inside them

    def shadow(self, mask, dy, blur, alpha, clip=None):
        s = mask.filter(ImageFilter.GaussianBlur(blur * U))
        moved = Image.new("L", (N, N), 0)  # shift down without wrapping the bottom rows round to the top
        moved.paste(s, (0, round(dy * U)))
        s = moved
        s = s.point(lambda v: round(v * alpha))
        if clip is not None:
            s = ImageChops.multiply(s, clip)
        layer = Image.new("RGBA", (N, N), (0, 0, 0, 0))
        layer.putalpha(s)
        self.img.alpha_composite(layer)

    def layer(self, mask, c1, c2, shadow="object"):
        if shadow == "object":  # a separate object resting on the surface: a soft shadow and a contact shadow
            self.shadow(mask, 1.0, 1.0, 0.28)
            self.shadow(mask, 0.0, 0.25, 0.18)
        elif shadow == "masked":  # overlaps a different metaphor: the shadow stays inside the shape below
            self.shadow(mask, 0.5, 0.75, 0.30, clip=self.below)
        elif shadow == "inner":  # part of the same metaphor: a tighter shadow, also kept inside
            self.shadow(mask, 0.25, 0.4, 0.22, clip=self.below)
        self.img.paste(gradient(c1, c2), (0, 0), mask)
        self.below = ImageChops.lighter(self.below, mask)


# ---------------------------------------------------------------- one function per app


def editor():
    # A bowl with an orange cube (3D) in it
    ic = Icon()
    top = [(24, 5), (36, 11), (24, 17), (12, 11)]
    left = [(12, 11), (24, 17), (24, 33), (12, 27)]
    right = [(24, 17), (36, 11), (36, 27), (24, 33)]
    outline = [(24, 5), (36, 11), (36, 27), (24, 33), (12, 27), (12, 11)]
    ic.shadow(shape(lambda d: d.polygon([tuple(p(*v)) for v in outline], fill=255)), 1.0, 1.0, 0.25)
    for face, c1, c2 in [(left, (255, 172, 82), (238, 128, 40)), (right, (222, 116, 38), (190, 88, 24)), (top, (255, 222, 150), (255, 192, 98))]:
        ic.layer(shape(lambda d, f=face: d.polygon([tuple(p(*v)) for v in f], fill=255)), c1, c2, shadow=None)
    bowl = shape(lambda d: (d.chord(p(6, 10, 42, 46), 0, 180, fill=255), d.rounded_rectangle(p(18.5, 39, 29.5, 43), radius=r(1), fill=255)))
    ic.layer(bowl, (240, 244, 250), (168, 182, 206))
    rim = shape(lambda d: d.rounded_rectangle(p(4.5, 25, 43.5, 30), radius=r(2), fill=255))
    ic.layer(rim, (255, 255, 255), (228, 234, 243), shadow="inner")
    return ic.img


def manager():
    # Hub: four tiles, one of them the editor's orange
    ic = Icon()
    cols = [((255, 196, 96), (240, 132, 40)), ((126, 178, 245), (60, 112, 210)), ((104, 154, 232), (44, 92, 188)), ((84, 132, 214), (32, 72, 160))]
    for i, (c1, c2) in enumerate(cols):
        x, y = 6 + (i % 2) * 19, 6 + (i // 2) * 19
        ic.layer(shape(lambda d, x=x, y=y: d.rounded_rectangle(p(x, y, x + 17, y + 17), radius=r(2.5), fill=255)), c1, c2)
    return ic.img


def audio():
    # Waveform bars on a green card
    ic = Icon()
    ic.layer(shape(lambda d: d.rounded_rectangle(p(4, 8, 44, 40), radius=r(4), fill=255)), (52, 190, 132), (14, 112, 82))
    heights = [6, 13, 21, 15, 9, 18, 11]
    w, gap = 3, 1.8
    x = 24 - (len(heights) * w + (len(heights) - 1) * gap) / 2
    bars = shape(lambda d: [d.rounded_rectangle(p(x + i * (w + gap), 24 - h / 2, x + i * (w + gap) + w, 24 + h / 2), radius=r(1.5), fill=255) for i, h in enumerate(heights)])
    ic.layer(bars, (250, 255, 252), (196, 240, 218), shadow="inner")
    return ic.img


def ui():
    # A window: purple title bar, a side column and two blocks
    ic = Icon()
    win = shape(lambda d: d.rounded_rectangle(p(4, 7, 44, 41), radius=r(3), fill=255))
    ic.layer(win, (252, 252, 254), (214, 220, 234))
    bar = ImageChops.multiply(win, shape(lambda d: d.rectangle(p(0, 0, 48, 14), fill=255)))
    ic.layer(bar, (196, 138, 250), (138, 78, 222), shadow=None)
    ic.layer(shape(lambda d: d.rounded_rectangle(p(8, 18, 18, 37), radius=r(1), fill=255)), (214, 172, 252), (170, 112, 236), shadow="inner")
    for y0, y1 in [(18, 26), (29, 37)]:
        ic.layer(shape(lambda d, a=y0, b=y1: d.rounded_rectangle(p(21, a, 40, b), radius=r(1), fill=255)), (178, 192, 222), (126, 144, 186), shadow="inner")
    return ic.img


def render():
    # A lit sphere (3D) over its ground shadow: diffuse light from the top left, a soft highlight, a faint rim below
    ic = Icon()
    ic.shadow(shape(lambda d: d.ellipse(p(12, 39, 36, 44), fill=255)), 0, 1.2, 0.35)
    cx, cy, rad = 24 * U, 22.5 * U, 16 * U
    yy, xx = np.mgrid[0:N, 0:N].astype(float)
    nx, ny = (xx - cx) / rad, (yy - cy) / rad
    q = nx * nx + ny * ny
    nz = np.sqrt(np.clip(1.0 - q, 0, 1))
    light = np.array([-0.45, -0.6, 0.66])
    light /= np.linalg.norm(light)
    diff = np.clip(nx * light[0] + ny * light[1] + nz * light[2], 0, 1)
    dark, bright = np.array([20, 104, 132.0]), np.array([120, 228, 236.0])
    rgb = dark + (bright - dark) * diff[..., None] ** 1.2
    rgb += (255 - rgb) * (diff**30 * 0.55)[..., None]
    rgb += (np.array([150, 240, 245.0]) - rgb) * (np.clip(ny, 0, 1) * np.clip(q, 0, 1) ** 3 * 0.25)[..., None]
    alpha = np.array(shape(lambda d: d.ellipse([cx - rad, cy - rad, cx + rad, cy + rad], fill=255)), float)
    ic.img.alpha_composite(Image.fromarray(np.dstack([np.clip(rgb, 0, 255), alpha]).astype(np.uint8)))
    return ic.img


def qa():
    # A clipboard with a check badge
    ic = Icon()
    ic.layer(shape(lambda d: d.rounded_rectangle(p(8, 7, 38, 44), radius=r(3), fill=255)), (104, 136, 188), (52, 78, 130))
    ic.layer(shape(lambda d: d.rounded_rectangle(p(11, 11, 35, 41), radius=r(1.5), fill=255)), (255, 255, 255), (222, 228, 238), shadow="inner")
    lines = shape(lambda d: [d.rounded_rectangle(p(15, y, 31 if i < 2 else 25, y + 2), radius=r(1), fill=255) for i, y in enumerate([17, 22, 27])])
    ic.layer(lines, (184, 196, 216), (160, 174, 198), shadow=None)
    ic.layer(shape(lambda d: d.rounded_rectangle(p(16, 4, 30, 11), radius=r(2), fill=255)), (92, 102, 120), (48, 56, 70), shadow="inner")
    ic.layer(shape(lambda d: d.ellipse(p(26, 27, 45, 46), fill=255)), (255, 212, 100), (236, 148, 28), shadow="masked")
    ends = [(30.5, 36.8), (40.5, 33)]
    check = shape(lambda d: (d.line([tuple(p(*ends[0])), tuple(p(34, 40)), tuple(p(*ends[1]))], fill=255, width=r(2.6), joint="curve"),
                             [d.ellipse(p(x - 1.3, y - 1.3, x + 1.3, y + 1.3), fill=255) for x, y in ends]))
    ic.layer(check, (255, 255, 255), (252, 244, 228), shadow="inner")
    return ic.img


# name -> (draw function, workspace-relative .ico path); the editor's is written by make_editor_icon.py
APPS = {
    "MealKitManager": (manager, os.path.join("MealKit-Manager", "res", "MealKitManager.ico")),
    "MkAudio": (audio, os.path.join("MkAudio", "tool", "res", "MkAudio.ico")),
    "MkUI": (ui, os.path.join("MkUI", "tool", "res", "MkUI.ico")),
    "MkRender": (render, os.path.join("MkRender", "tool", "res", "MkRender.ico")),
    "MkQA": (qa, os.path.join("MealKit-QA", "tool", "res", "MkQA.ico")),
}


def save_ico(big, path):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    images = [big.resize((sz, sz), Image.LANCZOS) for sz in SIZES]
    images[-1].save(path, format="ICO", sizes=[(sz, sz) for sz in SIZES], append_images=images[:-1])


def sheet(images, path):
    # Preview: every app on the Windows light and dark surfaces at 128, 64, 48, 32, 24 and 16 px
    show, cell = [128, 64, 48, 32, 24, 16], 150
    w, h = cell * len(show) * 2, cell * len(images)
    img = Image.new("RGBA", (w, h))
    ImageDraw.Draw(img).rectangle([0, 0, w // 2, h], fill=(243, 243, 243, 255))
    ImageDraw.Draw(img).rectangle([w // 2, 0, w, h], fill=(32, 32, 32, 255))
    for row, big in enumerate(images.values()):
        for half in range(2):
            for i, s in enumerate(show):
                x = half * w // 2 + i * cell + (cell - s) // 2
                img.alpha_composite(big.resize((s, s), Image.LANCZOS), (x, row * cell + (cell - s) // 2))
    img.save(path)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", help="write every icon into this folder instead of the repos")
    args = parser.parse_args()
    if args.out:
        images = {"MealKitEditor": editor()}
        images.update({name: fn() for name, (fn, _) in APPS.items()})
        for name, big in images.items():
            save_ico(big, os.path.join(args.out, name + ".ico"))
            big.resize((256, 256), Image.LANCZOS).save(os.path.join(args.out, name + ".png"))
        sheet(images, os.path.join(args.out, "sheet.png"))
        print("wrote", args.out)
        return
    for name, (fn, rel) in APPS.items():
        out = os.path.join(WORKSPACE, rel)
        save_ico(fn(), out)
        print("wrote", out)


if __name__ == "__main__":
    main()

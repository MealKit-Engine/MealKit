"""App icons for the MealKit suite tools (needs Pillow: pip install pillow). docs/TOOL_DESIGN.md section 5.

    python tools/make_suite_icons.py            -> writes each tool's .ico into its sibling repo (..\\<repo>\\...)
    python tools/make_suite_icons.py --out DIR  -> writes all of them into DIR instead (preview)

Same family as tools/make_editor_icon.py: the editor's blue rounded square, one mark per app in its own color.
Drawn at 1024 px and scaled down to each size.
"""

import argparse
import math
import os

from PIL import Image, ImageDraw

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WORKSPACE = os.path.dirname(ROOT)
SIZES = [16, 20, 24, 32, 40, 48, 64, 128, 256]
N = 1024
WHITE = (245, 247, 250, 255)


def lerp(a, b, t):
    return tuple(round(x + (y - x) * t) for x, y in zip(a, b))


def background():
    # The editor icon's background (make_editor_icon.py): blue darkening from top to bottom on a rounded square
    img = Image.new("RGBA", (N, N), (0, 0, 0, 0))
    grad = Image.new("RGBA", (N, N))
    gd = ImageDraw.Draw(grad)
    top, bottom = (84, 124, 186), (36, 56, 92)
    for y in range(N):
        gd.line([(0, y), (N, y)], fill=lerp(top, bottom, y / (N - 1)) + (255,))
    mask = Image.new("L", (N, N), 0)
    ImageDraw.Draw(mask).rounded_rectangle([24, 24, N - 24, N - 24], radius=200, fill=255)
    img.paste(grad, (0, 0), mask)
    return img


def manager(d):
    # Hub: four tiles, one of them the editor's orange
    s, gap, x0, y0 = 300, 64, 182, 182
    colors = [(255, 170, 70, 255), WHITE, WHITE, WHITE]
    for i, c in enumerate(colors):
        x = x0 + (i % 2) * (s + gap)
        y = y0 + (i // 2) * (s + gap)
        d.rounded_rectangle([x, y, x + s, y + s], radius=60, fill=c)


def audio(d):
    # Waveform bars
    heights = [220, 420, 640, 480, 300, 560, 360]
    w, gap = 76, 36
    x = (N - (len(heights) * w + (len(heights) - 1) * gap)) // 2
    for h in heights:
        d.rounded_rectangle([x, N // 2 - h // 2, x + w, N // 2 + h // 2], radius=38, fill=(110, 225, 160, 255))
        x += w + gap


def ui(d):
    # A screen layout: a frame with a title bar, a side column and two blocks
    d.rounded_rectangle([170, 210, 854, 814], radius=56, fill=WHITE)
    d.rounded_rectangle([170, 210, 854, 330], radius=56, fill=(200, 140, 245, 255))
    d.rectangle([170, 280, 854, 330], fill=(200, 140, 245, 255))
    d.rounded_rectangle([226, 384, 410, 758], radius=28, fill=(200, 140, 245, 255))
    d.rounded_rectangle([456, 384, 798, 560], radius=28, fill=(150, 170, 205, 255))
    d.rounded_rectangle([456, 600, 798, 758], radius=28, fill=(150, 170, 205, 255))


def render(d, img):
    # A lit sphere: radial shading from a light at the top left
    cx, cy, r = N // 2, N // 2, 330
    sphere = Image.new("RGBA", (N, N), (0, 0, 0, 0))
    px = sphere.load()
    lx, ly, lz = -0.5, -0.6, 0.62
    for y in range(cy - r, cy + r + 1):
        for x in range(cx - r, cx + r + 1):
            nx, ny = (x - cx) / r, (y - cy) / r
            q = nx * nx + ny * ny
            if q > 1.0:
                continue
            nz = math.sqrt(1.0 - q)
            diff = max(0.0, nx * lx + ny * ly + nz * lz)
            spec = max(0.0, nz * 0.9 + diff * 0.1) ** 40
            base = lerp((40, 120, 140), (110, 230, 235), diff)
            c = lerp(base, (255, 255, 255), min(1.0, spec))
            px[x, y] = c + (255,)
    img.alpha_composite(sphere)


def qa(d):
    # A check mark in a ring
    cx, cy, r = N // 2, N // 2, 330
    d.ellipse([cx - r, cy - r, cx + r, cy + r], outline=WHITE, width=70)
    d.line([(352, 530), (470, 648), (690, 410)], fill=(255, 200, 90, 255), width=96, joint="curve")
    for x, y in [(352, 530), (690, 410)]:
        d.ellipse([x - 48, y - 48, x + 48, y + 48], fill=(255, 200, 90, 255))


# name -> (draw function, repo-relative .ico path)
APPS = {
    "MealKitManager": (manager, os.path.join("MealKit-Manager", "res", "MealKitManager.ico")),
    "MkAudio": (audio, os.path.join("MkAudio", "tool", "res", "MkAudio.ico")),
    "MkUI": (ui, os.path.join("MkUI", "tool", "res", "MkUI.ico")),
    "MkRender": (render, os.path.join("MkRender", "tool", "res", "MkRender.ico")),
    "MkQA": (qa, os.path.join("MealKit-QA", "tool", "res", "MkQA.ico")),
}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", help="write every icon into this folder instead of the repos")
    args = parser.parse_args()
    for name, (fn, rel) in APPS.items():
        img = background()
        d = ImageDraw.Draw(img)
        if fn is render:
            fn(d, img)
        else:
            fn(d)
        out = os.path.join(args.out, name + ".ico") if args.out else os.path.join(WORKSPACE, rel)
        os.makedirs(os.path.dirname(out), exist_ok=True)
        images = [img.resize((sz, sz), Image.LANCZOS) for sz in SIZES]
        images[-1].save(out, format="ICO", sizes=[(sz, sz) for sz in SIZES], append_images=images[:-1])
        if args.out:
            img.resize((256, 256), Image.LANCZOS).save(os.path.join(args.out, name + ".png"))
        print("wrote", out)


if __name__ == "__main__":
    main()

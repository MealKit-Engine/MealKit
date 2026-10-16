"""MealKit 에디터 아이콘 생성기 (Pillow 필요: pip install pillow).

    python tools/make_editor_icon.py   -> engine/res/MealKitEditor.ico (창 아이콘), MealKitEditor.png (프로젝트 기본 아이콘)

파란 둥근 사각형 위에 그릇과 그 안의 주황 정육면체(3D)를 그린다. 1024px로 그리고 각 크기로 줄인다.
"""

import os

from PIL import Image, ImageDraw

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "engine", "res", "MealKitEditor.ico")
OUT_PNG = os.path.join(ROOT, "engine", "res", "MealKitEditor.png")
SIZES = [16, 20, 24, 32, 40, 48, 64, 128, 256]
N = 1024


def lerp(a, b, t):
    return tuple(round(x + (y - x) * t) for x, y in zip(a, b))


def draw():
    img = Image.new("RGBA", (N, N), (0, 0, 0, 0))

    # 배경: 위에서 아래로 짙어지는 파란색 (에디터 강조색 계열)
    grad = Image.new("RGBA", (N, N))
    gd = ImageDraw.Draw(grad)
    top, bottom = (84, 124, 186), (36, 56, 92)
    for y in range(N):
        gd.line([(0, y), (N, y)], fill=lerp(top, bottom, y / (N - 1)) + (255,))
    mask = Image.new("L", (N, N), 0)
    ImageDraw.Draw(mask).rounded_rectangle([24, 24, N - 24, N - 24], radius=200, fill=255)
    img.paste(grad, (0, 0), mask)

    d = ImageDraw.Draw(img)

    # 정육면체 (아이소메트릭): 중심 cx, 윗면 꼭짓점에서 아래로
    cx, cy, s = N // 2, 250, 230  # cy = 윗면 위 꼭짓점, s = 모서리 길이 비슷한 값
    h = s // 2
    top_face = [(cx, cy), (cx + s, cy + h), (cx, cy + 2 * h), (cx - s, cy + h)]
    left_face = [(cx - s, cy + h), (cx, cy + 2 * h), (cx, cy + 2 * h + s), (cx - s, cy + h + s)]
    right_face = [(cx + s, cy + h), (cx, cy + 2 * h), (cx, cy + 2 * h + s), (cx + s, cy + h + s)]
    d.polygon(top_face, fill=(255, 200, 110, 255))
    d.polygon(left_face, fill=(245, 145, 50, 255))
    d.polygon(right_face, fill=(205, 105, 30, 255))

    # 그릇: 정육면체 아래쪽을 가리는 흰 반원 + 테두리
    bx0, bx1 = 170, N - 170
    by = 600  # 그릇 윗선
    d.chord([bx0, by - (bx1 - bx0) // 2, bx1, by + (bx1 - bx0) // 2], 0, 180, fill=(245, 247, 250, 255))
    d.rounded_rectangle([bx0 - 30, by - 26, bx1 + 30, by + 26], radius=26, fill=(255, 255, 255, 255))
    # 그릇 받침
    d.rounded_rectangle([cx - 120, 900, cx + 120, 940], radius=20, fill=(245, 247, 250, 255))
    return img


def main():
    big = draw()
    images = [big.resize((sz, sz), Image.LANCZOS) for sz in SIZES]
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    images[-1].save(OUT, format="ICO", sizes=[(sz, sz) for sz in SIZES], append_images=images[:-1])
    big.resize((256, 256), Image.LANCZOS).save(OUT_PNG)
    print("wrote", OUT, OUT_PNG)


if __name__ == "__main__":
    main()

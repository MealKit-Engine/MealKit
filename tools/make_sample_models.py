"""Aster 샘플 모델 생성기: 표준 라이브러리만으로 glTF 2.0 바이너리(.glb)를 만든다.

    python tools/make_sample_models.py            -> assets/models/{tree,rock,ring}.glb

glTF 규칙(오른손 좌표계, y 위, 앞면 = 반시계)으로 저장한다. Aster가 불러올 때 왼손 좌표계로 바꾼다.
"""

import json
import math
import os
import struct

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_DIR = os.path.join(ROOT, "assets", "models")


def sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def normalize(v):
    length = math.sqrt(dot(v, v)) or 1.0
    return (v[0] / length, v[1] / length, v[2] / length)


class Part:
    """한 재질(색)을 쓰는 삼각형 묶음 = glTF primitive 하나."""

    def __init__(self, color):
        self.color = color
        self.positions = []
        self.normals = []
        self.indices = []

    def flat_triangle(self, a, b, c, inside):
        """각진 삼각형. inside(물체 안쪽 점)를 기준으로 바깥을 보게 감는 순서를 맞춘다."""
        n = cross(sub(b, a), sub(c, a))
        centroid = ((a[0] + b[0] + c[0]) / 3, (a[1] + b[1] + c[1]) / 3, (a[2] + b[2] + c[2]) / 3)
        if dot(n, sub(centroid, inside)) < 0:
            b, c = c, b
            n = (-n[0], -n[1], -n[2])
        n = normalize(n)
        base = len(self.positions)
        for p in (a, b, c):
            self.positions.append(p)
            self.normals.append(n)
        self.indices += [base, base + 1, base + 2]


def write_glb(path, name, parts):
    blob = bytearray()
    views, accessors, materials, primitives = [], [], [], []

    def add_view(data, target):
        while len(blob) % 4:
            blob.append(0)
        views.append({"buffer": 0, "byteOffset": len(blob), "byteLength": len(data), "target": target})
        blob.extend(data)
        return len(views) - 1

    for i, part in enumerate(parts):
        flat_pos = [c for p in part.positions for c in p]
        flat_nrm = [c for n in part.normals for c in n]
        pos_view = add_view(struct.pack("<%df" % len(flat_pos), *flat_pos), 34962)
        lo = [min(p[k] for p in part.positions) for k in range(3)]
        hi = [max(p[k] for p in part.positions) for k in range(3)]
        accessors.append({"bufferView": pos_view, "componentType": 5126, "count": len(part.positions),
                          "type": "VEC3", "min": lo, "max": hi})
        pos_acc = len(accessors) - 1
        nrm_view = add_view(struct.pack("<%df" % len(flat_nrm), *flat_nrm), 34962)
        accessors.append({"bufferView": nrm_view, "componentType": 5126, "count": len(part.normals), "type": "VEC3"})
        nrm_acc = len(accessors) - 1
        idx_view = add_view(struct.pack("<%dH" % len(part.indices), *part.indices), 34963)
        accessors.append({"bufferView": idx_view, "componentType": 5123, "count": len(part.indices), "type": "SCALAR"})
        idx_acc = len(accessors) - 1
        materials.append({"name": "%s_%d" % (name, i),
                          "pbrMetallicRoughness": {"baseColorFactor": list(part.color) + [1.0],
                                                   "metallicFactor": 0.0, "roughnessFactor": 0.9}})
        primitives.append({"attributes": {"POSITION": pos_acc, "NORMAL": nrm_acc}, "indices": idx_acc,
                           "material": len(materials) - 1})

    while len(blob) % 4:
        blob.append(0)
    gltf = {
        "asset": {"version": "2.0", "generator": "Aster make_sample_models.py"},
        "scene": 0, "scenes": [{"nodes": [0]}], "nodes": [{"mesh": 0, "name": name}],
        "meshes": [{"name": name, "primitives": primitives}], "materials": materials,
        "buffers": [{"byteLength": len(blob)}], "bufferViews": views, "accessors": accessors,
    }
    js = json.dumps(gltf, separators=(",", ":")).encode("utf-8")
    js += b" " * ((4 - len(js) % 4) % 4)
    total = 12 + 8 + len(js) + 8 + len(blob)
    with open(path, "wb") as f:
        f.write(struct.pack("<III", 0x46546C67, 2, total))
        f.write(struct.pack("<II", len(js), 0x4E4F534A))
        f.write(js)
        f.write(struct.pack("<II", len(blob), 0x004E4942))
        f.write(blob)
    tris = sum(len(p.indices) for p in parts) // 3
    print("wrote %s (%d triangles, %d bytes)" % (os.path.relpath(path, ROOT), tris, total))


def ring_points(radius, y, sides, phase=0.0):
    return [(radius * math.cos(phase + 2 * math.pi * i / sides), y, radius * math.sin(phase + 2 * math.pi * i / sides))
            for i in range(sides)]


def make_tree():
    trunk = Part((0.30, 0.17, 0.08))
    sides = 7
    bottom, top = ring_points(0.18, 0.0, sides), ring_points(0.14, 1.4, sides)
    inside = (0.0, 0.7, 0.0)
    for i in range(sides):
        j = (i + 1) % sides
        trunk.flat_triangle(bottom[i], bottom[j], top[j], inside)
        trunk.flat_triangle(bottom[i], top[j], top[i], inside)
    leaves = Part((0.12, 0.40, 0.10))
    for base_y, radius, height, phase in ((1.0, 1.1, 1.6, 0.0), (1.9, 0.8, 1.3, 0.4)):
        base = ring_points(radius, base_y, 9, phase)
        apex = (0.0, base_y + height, 0.0)
        center = (0.0, base_y, 0.0)
        inside = (0.0, base_y + height * 0.3, 0.0)
        for i in range(9):
            j = (i + 1) % 9
            leaves.flat_triangle(base[i], base[j], apex, inside)
            leaves.flat_triangle(base[i], base[j], center, inside)
    write_glb(os.path.join(OUT_DIR, "tree.glb"), "tree", [trunk, leaves])


def make_rock():
    t = (1 + math.sqrt(5)) / 2
    verts = [normalize(v) for v in [(-1, t, 0), (1, t, 0), (-1, -t, 0), (1, -t, 0), (0, -1, t), (0, 1, t),
                                    (0, -1, -t), (0, 1, -t), (t, 0, -1), (t, 0, 1), (-t, 0, -1), (-t, 0, 1)]]
    faces = [(0, 11, 5), (0, 5, 1), (0, 1, 7), (0, 7, 10), (0, 10, 11), (1, 5, 9), (5, 11, 4), (11, 10, 2),
             (10, 7, 6), (7, 1, 8), (3, 9, 4), (3, 4, 2), (3, 2, 6), (3, 6, 8), (3, 8, 9), (4, 9, 5), (2, 4, 11),
             (6, 2, 10), (8, 6, 7), (9, 8, 1)]
    for _ in range(2):  # 정이십면체를 두 번 나눠 320면
        cache, new_faces = {}, []

        def midpoint(a, b):
            key = (min(a, b), max(a, b))
            if key not in cache:
                verts.append(normalize(tuple((verts[a][k] + verts[b][k]) / 2 for k in range(3))))
                cache[key] = len(verts) - 1
            return cache[key]

        for a, b, c in faces:
            ab, bc, ca = midpoint(a, b), midpoint(b, c), midpoint(c, a)
            new_faces += [(a, ab, ca), (b, bc, ab), (c, ca, bc), (ab, bc, ca)]
        faces = new_faces
    # 결정론적인 울퉁불퉁함 (난수 대신 정수 해시)
    shaped = []
    for i, v in enumerate(verts):
        h = ((i * 2654435761) & 0xFFFFFFFF) / 0xFFFFFFFF
        r = 0.82 + 0.3 * h
        shaped.append((v[0] * r * 1.25, v[1] * r * 0.8 + 0.6, v[2] * r))
    rock = Part((0.42, 0.40, 0.37))
    for a, b, c in faces:
        rock.flat_triangle(shaped[a], shaped[b], shaped[c], (0.0, 0.6, 0.0))
    write_glb(os.path.join(OUT_DIR, "rock.glb"), "rock", [rock])


def make_ring():
    """세워진 고리(토러스). 매끈한 법선, 삼각형 3072개 → LOD 시험용."""
    ring = Part((0.85, 0.60, 0.12))
    major, minor, seg_u, seg_v = 1.0, 0.3, 64, 24
    lift = major + minor
    grid = []
    for i in range(seg_u):
        u = 2 * math.pi * i / seg_u
        row = []
        for j in range(seg_v):
            v = 2 * math.pi * j / seg_v
            cx, cy = major * math.cos(u), major * math.sin(u)
            nx, ny, nz = math.cos(v) * math.cos(u), math.cos(v) * math.sin(u), math.sin(v)
            ring.positions.append((cx + minor * nx, cy + minor * ny + lift, minor * nz))
            ring.normals.append((nx, ny, nz))
            row.append(len(ring.positions) - 1)
        grid.append(row)
    for i in range(seg_u):
        for j in range(seg_v):
            a, b = grid[i][j], grid[(i + 1) % seg_u][j]
            c, d = grid[(i + 1) % seg_u][(j + 1) % seg_v], grid[i][(j + 1) % seg_v]
            for tri in ((a, b, c), (a, c, d)):
                p0, p1, p2 = (ring.positions[k] for k in tri)
                n = cross(sub(p1, p0), sub(p2, p0))
                if dot(n, ring.normals[tri[0]]) < 0:  # 바깥이 반시계가 되게
                    tri = (tri[0], tri[2], tri[1])
                ring.indices += list(tri)
    write_glb(os.path.join(OUT_DIR, "ring.glb"), "ring", [ring])


if __name__ == "__main__":
    os.makedirs(OUT_DIR, exist_ok=True)
    make_tree()
    make_rock()
    make_ring()

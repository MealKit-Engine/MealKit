"""MealKit sample model generator: makes glTF 2.0 binaries (.glb) with only the standard library.

    python tools/make_sample_models.py            -> assets/models/{tree,rock,ring}.glb, models/props/{crate,fence,style_*}.glb

tree/rock/ring use only material colors without vertex colors (used by scenes and fixed recordings; changing them changes the content hash), crate/fence are texture
examples (glTF PBR: base color/normal/metal-roughness textures, alpha mask. assets/scenes/materials.scene, graphics regression tests).

Saved with glTF rules (right-handed, y up, front face = counterclockwise). MealKit converts to left-handed when loading.
"""

import json
import math
import os
import struct
import zlib

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
    """A group of triangles using one material (color) = one glTF primitive."""

    def __init__(self, color):
        self.color = color
        self.positions = []
        self.normals = []
        self.indices = []

    def flat_triangle(self, a, b, c, inside):
        """Faceted triangles. Winding is fixed to face outward relative to inside (a point inside the object)."""
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
        "asset": {"version": "2.0", "generator": "MealKit make_sample_models.py"},
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
    for _ in range(2):  # subdivide an icosahedron twice for 320 faces
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
    # Deterministic bumpiness (integer hash instead of random numbers)
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
    """An upright ring (torus). Smooth normals, 3072 triangles → for LOD tests."""
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
                if dot(n, ring.normals[tri[0]]) < 0:  # so the outside is counterclockwise
                    tri = (tri[0], tri[2], tri[1])
                ring.indices += list(tri)
    write_glb(os.path.join(OUT_DIR, "ring.glb"), "ring", [ring])


# ---------------------------------------------------------------- Texture examples (crate, fence)

def png_bytes(width, height, pixel):
    """Makes an RGBA PNG from pixel(x, y) -> (r, g, b, a) 0..255 (uses only zlib)."""
    rows = bytearray()
    for y in range(height):
        rows.append(0)
        for x in range(width):
            rows.extend(bytes(pixel(x, y)))

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)

    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)) +
            chunk(b"IDAT", zlib.compress(bytes(rows), 9)) + chunk(b"IEND", b""))


def hash01(x, y, seed):
    """A fixed 0..1 value per coordinate (wood grain noise)."""
    h = (x * 374761393 + y * 668265263 + seed * 2147483647) & 0xFFFFFFFF
    h = ((h ^ (h >> 13)) * 1274126177) & 0xFFFFFFFF
    return ((h ^ (h >> 16)) & 0xFFFF) / 65535.0


def crate_textures(size=256):
    """Wooden planks + metal frame. Base color, normals (plank grooves/frame bevels), metal-roughness (G = roughness, B = metal)."""
    border = size // 10
    plank = size // 4

    def frame(x, y):
        return min(x, y, size - 1 - x, size - 1 - y) < border

    def base(x, y):
        if frame(x, y):
            v = 150 + int(20 * hash01(x, y, 3))
            return (v, v, v + 8, 255)
        grain = 0.8 + 0.2 * math.sin(y * 0.22 + 0.6 * math.sin(x * 0.15) + 6 * hash01(x // 64, 0, 1))
        v = 0.85 + 0.15 * hash01(x, y, 2)
        return (int(170 * grain * v), int(110 * grain * v), int(60 * grain * v), 255)

    def height(x, y):
        if frame(x, y):
            return 1.0
        groove = (x % plank)
        return 0.0 if groove < 3 else 0.5

    def normal(x, y):
        dx = height(min(x + 1, size - 1), y) - height(max(x - 1, 0), y)
        dy = height(x, min(y + 1, size - 1)) - height(x, max(y - 1, 0))
        n = normalize((-dx * 2.0, -dy * 2.0, 1.0))
        return (int((n[0] * 0.5 + 0.5) * 255), int((n[1] * 0.5 + 0.5) * 255), int((n[2] * 0.5 + 0.5) * 255), 255)

    def metal_rough(x, y):
        return (255, 90, 255, 255) if frame(x, y) else (255, 215, 0, 255)

    return png_bytes(size, size, base), png_bytes(size, size, normal), png_bytes(size, size, metal_rough)


def fence_texture(size=128):
    """Wire mesh: holes punched with an alpha mask."""
    cell = size // 8

    def pixel(x, y):
        wire = (x % cell) < 3 or (y % cell) < 3
        return (90, 95, 100, 255) if wire else (0, 0, 0, 0)

    return png_bytes(size, size, pixel)


def write_textured_glb(path, name, positions, normals, uvs, indices, material, images):
    """One mesh with position/normal/UV per vertex and one textured material. images: list of PNG bytes (in texture index order)."""
    blob = bytearray()
    views, accessors = [], []

    def add_view(data, target=None):
        while len(blob) % 4:
            blob.append(0)
        view = {"buffer": 0, "byteOffset": len(blob), "byteLength": len(data)}
        if target:
            view["target"] = target
        views.append(view)
        blob.extend(data)
        return len(views) - 1

    def add_accessor(fmt, values, count, kind, comp=5126, extra=None):
        accessors.append(dict({"bufferView": add_view(struct.pack("<%d%s" % (len(values), fmt), *values),
                                                       34963 if fmt == "H" else 34962),
                               "componentType": comp, "count": count, "type": kind}, **(extra or {})))
        return len(accessors) - 1

    flat = [c for p in positions for c in p]
    lo = [min(p[k] for p in positions) for k in range(3)]
    hi = [max(p[k] for p in positions) for k in range(3)]
    pos = add_accessor("f", flat, len(positions), "VEC3", extra={"min": lo, "max": hi})
    nrm = add_accessor("f", [c for n in normals for c in n], len(normals), "VEC3")
    uv = add_accessor("f", [c for t in uvs for c in t], len(uvs), "VEC2")
    idx = add_accessor("H", indices, len(indices), "SCALAR", comp=5123)
    image_list = [{"bufferView": add_view(png), "mimeType": "image/png"} for png in images]
    while len(blob) % 4:
        blob.append(0)
    gltf = {
        "asset": {"version": "2.0", "generator": "MealKit make_sample_models.py"},
        "scene": 0, "scenes": [{"nodes": [0]}], "nodes": [{"mesh": 0, "name": name}],
        "meshes": [{"name": name, "primitives": [{"attributes": {"POSITION": pos, "NORMAL": nrm, "TEXCOORD_0": uv},
                                                  "indices": idx, "material": 0}]}],
        "materials": [material], "images": image_list,
        "samplers": [{"magFilter": 9729, "minFilter": 9987, "wrapS": 10497, "wrapT": 10497}],
        "textures": [{"sampler": 0, "source": i} for i in range(len(images))],
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
    print("wrote %s (%d triangles, %d bytes)" % (os.path.relpath(path, ROOT), len(indices) // 3, total))


def make_crate():
    """A 1 m box, the whole texture on each face (glTF: right-handed, counterclockwise front faces)."""
    positions, normals, uvs, indices = [], [], [], []
    faces = [((1, 0, 0), (0, 0, -1)), ((-1, 0, 0), (0, 0, 1)), ((0, 1, 0), (1, 0, 0)),
             ((0, -1, 0), (1, 0, 0)), ((0, 0, 1), (1, 0, 0)), ((0, 0, -1), (-1, 0, 0))]
    for n, u in faces:
        v = cross(n, u)  # right-handed: if u × v = n, counterclockwise is the front face
        c = (n[0] * 0.5, n[1] * 0.5, n[2] * 0.5)
        base = len(positions)
        for su, sv, tu, tv in ((-1, -1, 0, 1), (1, -1, 1, 1), (1, 1, 1, 0), (-1, 1, 0, 0)):
            positions.append((c[0] + (u[0] * su + v[0] * sv) * 0.5, c[1] + (u[1] * su + v[1] * sv) * 0.5,
                              c[2] + (u[2] * su + v[2] * sv) * 0.5))
            normals.append(n)
            uvs.append((tu, tv))
        indices += [base, base + 1, base + 2, base, base + 2, base + 3]
    base_png, normal_png, mr_png = crate_textures()
    material = {"name": "crate", "pbrMetallicRoughness": {"baseColorTexture": {"index": 0}, "metallicFactor": 1.0,
                                                          "roughnessFactor": 1.0, "metallicRoughnessTexture": {"index": 2}},
                "normalTexture": {"index": 1}}
    out = os.path.join(OUT_DIR, "props")
    os.makedirs(out, exist_ok=True)
    write_textured_glb(os.path.join(out, "crate.glb"), "crate", positions, normals, uvs, indices, material,
                       [base_png, normal_png, mr_png])


def make_fence():
    """A 2 m wide, 1 m tall wire mesh panel (two faces front and back, alpha mask)."""
    positions, normals, uvs, indices = [], [], [], []
    for n in ((0, 0, 1), (0, 0, -1)):
        base = len(positions)
        x = 1.0 if n[2] > 0 else -1.0
        for px, py, tu, tv in ((-x, 0, 0, 1), (x, 0, 2, 1), (x, 1, 2, 0), (-x, 1, 0, 0)):
            positions.append((px, py, n[2] * 0.01))
            normals.append(n)
            uvs.append((tu, tv))
        indices += [base, base + 1, base + 2, base, base + 2, base + 3]
    material = {"name": "fence", "alphaMode": "MASK", "alphaCutoff": 0.5,
                "pbrMetallicRoughness": {"baseColorTexture": {"index": 0}, "metallicFactor": 0.8, "roughnessFactor": 0.4}}
    write_textured_glb(os.path.join(OUT_DIR, "props", "fence.glb"), "fence", positions, normals, uvs, indices, material,
                       [fence_texture()])


# ---------------------------------------------------------------- Style preset examples (outline/rim/wind/dissolve)
# Material features are glTF extras "mealkit_<key>" (same as the Blender add-on's custom properties). assets/scenes/styles.scene, graphics regression tests

def write_style_glb(path, name, parts):
    """parts: list of (positions, normals, indices, colors (RGBA or None), material). Multiple primitives in one mesh."""
    blob = bytearray()
    views, accessors, materials, primitives = [], [], [], []

    def add(data, target, count, kind, ctype=5126, extra=None):
        while len(blob) % 4:
            blob.append(0)
        views.append({"buffer": 0, "byteOffset": len(blob), "byteLength": len(data), "target": target})
        blob.extend(data)
        acc = {"bufferView": len(views) - 1, "componentType": ctype, "count": count, "type": kind}
        acc.update(extra or {})
        accessors.append(acc)
        return len(accessors) - 1

    for positions, normals, indices, colors, material in parts:
        flat = [c for p in positions for c in p]
        lo = [min(p[k] for p in positions) for k in range(3)]
        hi = [max(p[k] for p in positions) for k in range(3)]
        attrs = {"POSITION": add(struct.pack("<%df" % len(flat), *flat), 34962, len(positions), "VEC3", extra={"min": lo, "max": hi}),
                 "NORMAL": add(struct.pack("<%df" % (3 * len(normals)), *[c for n in normals for c in n]), 34962, len(normals), "VEC3")}
        if colors:
            attrs["COLOR_0"] = add(struct.pack("<%df" % (4 * len(colors)), *[c for col in colors for c in col]), 34962,
                                   len(colors), "VEC4")
        idx = add(struct.pack("<%dH" % len(indices), *indices), 34963, len(indices), "SCALAR", ctype=5123)
        materials.append(material)
        primitives.append({"attributes": attrs, "indices": idx, "material": len(materials) - 1})

    while len(blob) % 4:
        blob.append(0)
    gltf = {
        "asset": {"version": "2.0", "generator": "MealKit make_sample_models.py"},
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
    tris = sum(len(p[2]) for p in parts) // 3
    print("wrote %s (%d triangles, %d bytes)" % (os.path.relpath(path, ROOT), tris, total))


def smooth_sphere(center, radius, rings=16, segments=24, squash=1.0):
    """A sphere with smooth normals (vertices aren't split so the outline hull doesn't crack). glTF front face = counterclockwise."""
    positions, normals, indices = [], [], []
    for r in range(rings + 1):
        theta = math.pi * r / rings
        for s in range(segments + 1):
            phi = 2 * math.pi * s / segments
            n = (math.sin(theta) * math.cos(phi), math.cos(theta), math.sin(theta) * math.sin(phi))
            positions.append((center[0] + radius * n[0], center[1] + radius * squash * n[1], center[2] + radius * n[2]))
            normals.append(normalize((n[0], n[1] / squash, n[2])))
    for r in range(rings):
        for s in range(segments):
            a = r * (segments + 1) + s
            b = a + segments + 1
            indices += [a, a + 1, b, a + 1, b + 1, b]
    return positions, normals, indices


def pbr(name, color, roughness=0.6, metallic=0.0, **extras):
    m = {"name": name, "pbrMetallicRoughness": {"baseColorFactor": list(color) + [1.0], "metallicFactor": metallic,
                                                "roughnessFactor": roughness}}
    if extras:
        m["extras"] = {"mealkit_" + k: v for k, v in extras.items()}
    return m


def make_styles():
    out = os.path.join(OUT_DIR, "props")
    os.makedirs(out, exist_ok=True)
    # Figure: body (outline + rim) + head (outline only, thick) — shows best under toon lighting
    body = smooth_sphere((0.0, 0.75, 0.0), 0.5, squash=1.4)
    head = smooth_sphere((0.0, 1.75, 0.0), 0.35)
    write_style_glb(os.path.join(out, "style_figure.glb"), "style_figure", [
        body + (None, pbr("figure_body", (0.85, 0.35, 0.25), outline=0.03, outlineColor=[0.05, 0.02, 0.02], rim=0.8,
                         rimColor=[1.0, 0.85, 0.6])),
        head + (None, pbr("figure_head", (0.95, 0.8, 0.6), roughness=0.4, outline=0.04)),
    ])
    # Grass: several thin blades, vertex alpha = wind strength (root 0 → tip 1). Two sheets front and back so both sides show
    positions, normals, indices, colors = [], [], [], []
    for i in range(14):
        h = ((i * 2654435761) & 0xFFFFFFFF) / 0xFFFFFFFF
        angle = 2 * math.pi * i / 14 + h
        x, z = 0.45 * math.cos(angle) * (0.3 + h), 0.45 * math.sin(angle) * (0.3 + h)
        side = (0.06 * math.cos(angle + math.pi / 2), 0.06 * math.sin(angle + math.pi / 2))
        height = 0.7 + 0.5 * h
        tip = (x + 0.15 * math.cos(angle), height, z + 0.15 * math.sin(angle))
        n = normalize((math.cos(angle), 0.3, math.sin(angle)))
        for flip in (1, -1):
            base = len(positions)
            positions += [(x - side[0], 0.0, z - side[1]), (x + side[0], 0.0, z + side[1]), tip]
            normals += [tuple(c * flip for c in n)] * 3
            colors += [(0.35, 0.75, 0.25, 0.0), (0.35, 0.75, 0.25, 0.0), (0.75, 0.95, 0.4, 1.0)]
            indices += [base, base + 1, base + 2] if flip == 1 else [base, base + 2, base + 1]
    write_style_glb(os.path.join(out, "style_grass.glb"), "style_grass", [
        (positions, normals, indices, colors, pbr("grass", (1.0, 1.0, 1.0), roughness=0.8, wind=0.12)),
    ])
    # Bead: dissolve (half vanished by the progress in the scene object's flags) + rim
    orb = smooth_sphere((0.0, 0.6, 0.0), 0.6)
    write_style_glb(os.path.join(out, "style_orb.glb"), "style_orb", [
        orb + (None, pbr("orb", (0.3, 0.45, 0.9), roughness=0.3, metallic=0.5, dissolve=0.08,
                        dissolveColor=[3.0, 1.2, 0.3], rim=0.6, rimColor=[0.6, 0.8, 1.0])),
    ])


def make_probe_ball():
    # Mirror bead for checking reflection probes (assets/scenes/probes.scene): metal 1, roughness 0.08, so the surroundings reflect as is
    out = os.path.join(OUT_DIR, "props")
    os.makedirs(out, exist_ok=True)
    ball = smooth_sphere((0.0, 0.5, 0.0), 0.5)
    write_style_glb(os.path.join(out, "probe_ball.glb"), "probe_ball", [
        ball + (None, pbr("chrome", (0.95, 0.95, 0.95), roughness=0.08, metallic=1.0)),
    ])


def ellipsoid(center, radii, rings=12, segments=16):
    """A smooth ellipsoid with a different radius per axis. glTF front face = counterclockwise."""
    positions, normals, indices = [], [], []
    for r in range(rings + 1):
        theta = math.pi * r / rings
        for s in range(segments + 1):
            phi = 2 * math.pi * s / segments
            n = (math.sin(theta) * math.cos(phi), math.cos(theta), math.sin(theta) * math.sin(phi))
            positions.append(tuple(center[k] + radii[k] * n[k] for k in range(3)))
            normals.append(normalize((n[0] / radii[0], n[1] / radii[1], n[2] / radii[2])))
    for r in range(rings):
        for s in range(segments):
            a = r * (segments + 1) + s
            b = a + segments + 1
            indices += [a, a + 1, b, a + 1, b + 1, b]
    return positions, normals, indices


def quat_axis(axis, degrees):
    a = math.radians(degrees) * 0.5
    n = normalize(axis)
    return (n[0] * math.sin(a), n[1] * math.sin(a), n[2] * math.sin(a), math.cos(a))


def make_rig():
    # Figure for checking skeletal animation (sandbox's Pawn, scenes/anim.scene): 8 joints, clips idle/run/wave.
    # Each part is attached to one joint, only the torso blends between hips and spine (checks weight blending). In glTF forward is -z (+z in MealKit)
    out = os.path.join(OUT_DIR, "characters")
    os.makedirs(out, exist_ok=True)
    # Joints: name, parent, position relative to parent
    joints = [("root", -1, (0, 0, 0)), ("hips", 0, (0, 0.7, 0)), ("spine", 1, (0, 0.3, 0)), ("head", 2, (0, 0.5, 0)),
              ("arm.L", 2, (0.3, 0.25, 0)), ("arm.R", 2, (-0.3, 0.25, 0)), ("leg.L", 1, (0.15, 0, 0)), ("leg.R", 1, (-0.15, 0, 0))]
    world = []
    for name, parent, t in joints:
        world.append(tuple(t[k] + (world[parent][k] if parent >= 0 else 0) for k in range(3)))
    J = {name: i for i, (name, _, _) in enumerate(joints)}
    parts = []  # (positions, normals, indices, joint (or None = torso), material)
    skin_mat = pbr("rig_skin", (0.95, 0.75, 0.6), roughness=0.5)
    cloth = pbr("rig_cloth", (0.92, 0.92, 0.95), roughness=0.7)  # white so the instance color (player color) shows as is
    dark = pbr("rig_dark", (0.15, 0.15, 0.2), roughness=0.6)
    parts.append(ellipsoid((0, 1.0, 0), (0.28, 0.36, 0.2)) + (None, cloth))
    parts.append(ellipsoid((0, 1.62, 0), (0.2, 0.22, 0.2)) + ("head", skin_mat))
    parts.append(ellipsoid((0, 1.6, -0.2), (0.05, 0.05, 0.05)) + ("head", dark))  # nose: marks the front
    for side, sx in (("L", 1), ("R", -1)):
        parts.append(ellipsoid((0.4 * sx, 0.95, 0), (0.08, 0.32, 0.08)) + ("arm." + side, skin_mat))
        parts.append(ellipsoid((0.15 * sx, 0.38, 0), (0.1, 0.36, 0.1)) + ("leg." + side, dark))
    blob = bytearray()
    views, accessors, materials, primitives = [], [], [], []

    def add(data, target, count, kind, ctype=5126, extra=None):
        while len(blob) % 4:
            blob.append(0)
        view = {"buffer": 0, "byteOffset": len(blob), "byteLength": len(data)}
        if target:
            view["target"] = target
        views.append(view)
        blob.extend(data)
        acc = {"bufferView": len(views) - 1, "componentType": ctype, "count": count, "type": kind}
        acc.update(extra or {})
        accessors.append(acc)
        return len(accessors) - 1

    tris = 0
    for positions, normals, indices, joint, material in parts:
        js, ws = [], []
        for p in positions:
            if joint:
                js += [J[joint], 0, 0, 0]
                ws += [1.0, 0.0, 0.0, 0.0]
            else:  # torso: below 0.85 is hips, above 1.05 is spine, blended in between
                t = min(1.0, max(0.0, (p[1] - 0.85) / 0.2))
                js += [J["hips"], J["spine"], 0, 0]
                ws += [1.0 - t, t, 0.0, 0.0]
        flat = [c for p in positions for c in p]
        lo = [min(p[k] for p in positions) for k in range(3)]
        hi = [max(p[k] for p in positions) for k in range(3)]
        attrs = {"POSITION": add(struct.pack("<%df" % len(flat), *flat), 34962, len(positions), "VEC3", extra={"min": lo, "max": hi}),
                 "NORMAL": add(struct.pack("<%df" % (3 * len(normals)), *[c for n in normals for c in n]), 34962, len(normals), "VEC3"),
                 "JOINTS_0": add(struct.pack("<%dB" % len(js), *js), 34962, len(positions), "VEC4", ctype=5121),
                 "WEIGHTS_0": add(struct.pack("<%df" % len(ws), *ws), 34962, len(positions), "VEC4")}
        idx = add(struct.pack("<%dH" % len(indices), *indices), 34963, len(indices), "SCALAR", ctype=5123)
        if not any(m is material for m in materials):  # the same material only once (budget: material count)
            materials.append(material)
        primitives.append({"attributes": attrs, "indices": idx, "material": next(i for i, m in enumerate(materials) if m is material)})
        tris += len(indices) // 3
    ibm = []
    for w in world:  # inverse bind = inverse of the bind pose joint world (translation only), column-major
        ibm += [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, -w[0], -w[1], -w[2], 1]
    ibm_acc = add(struct.pack("<%df" % len(ibm), *ibm), None, len(world), "MAT4")
    nodes = [{"name": "rig_figure", "mesh": 0, "skin": 0}]
    for i, (name, parent, t) in enumerate(joints):
        node = {"name": name, "translation": list(t)}
        children = [1 + k for k, j in enumerate(joints) if j[1] == i]
        if children:
            node["children"] = children
        nodes.append(node)

    # Clips: (joint, path, axis, (key time, value)). 10 Hz keys, linear, last key = first key (looping)
    def wave(period, amp, offset=0.0, absolute=False):
        steps = max(2, int(round(period * 10)))
        times = [period * k / steps for k in range(steps + 1)]
        values = [amp * math.sin(2 * math.pi * t / period) for t in times]
        if absolute:
            values = [abs(v) for v in values]
        return times, [v + offset for v in values]

    clips = {
        "idle": [("spine", "rotation", (0, 0, 1), wave(2.0, 3.0)), ("arm.L", "rotation", (0, 0, 1), wave(2.0, 4.0)),
                 ("arm.R", "rotation", (0, 0, 1), wave(2.0, -4.0)), ("head", "rotation", (0, 1, 0), wave(4.0, 8.0))],
        "run": [("leg.L", "rotation", (1, 0, 0), wave(0.6, 35.0)), ("leg.R", "rotation", (1, 0, 0), wave(0.6, -35.0)),
                ("arm.L", "rotation", (1, 0, 0), wave(0.6, -40.0)), ("arm.R", "rotation", (1, 0, 0), wave(0.6, 40.0)),
                ("spine", "rotation", (1, 0, 0), wave(0.6, 0.0, offset=-8.0)),
                ("hips", "translation", None, wave(0.3, 0.05, absolute=True))],
        "wave": [("arm.R", "rotation", (0, 0, 1), wave(0.8, 20.0, offset=-150.0)), ("head", "rotation", (0, 0, 1), wave(0.8, 5.0))],
    }
    animations = []
    for clip_name, channels in clips.items():
        samplers, chans = [], []
        for joint, path, axis, (times, values) in channels:
            t_acc = add(struct.pack("<%df" % len(times), *times), None, len(times), "SCALAR", extra={"min": [min(times)], "max": [max(times)]})
            if path == "rotation":
                vals = [c for v in values for c in quat_axis(axis, v)]
                v_acc = add(struct.pack("<%df" % len(vals), *vals), None, len(values), "VEC4")
            else:
                base = joints[J[joint]][2]
                vals = [c for v in values for c in (base[0], base[1] + v, base[2])]
                v_acc = add(struct.pack("<%df" % len(vals), *vals), None, len(values), "VEC3")
            samplers.append({"input": t_acc, "output": v_acc, "interpolation": "LINEAR"})
            chans.append({"sampler": len(samplers) - 1, "target": {"node": 1 + J[joint], "path": path}})
        animations.append({"name": clip_name, "samplers": samplers, "channels": chans})
    while len(blob) % 4:
        blob.append(0)
    gltf = {
        "asset": {"version": "2.0", "generator": "MealKit make_sample_models.py"},
        "scene": 0, "scenes": [{"nodes": [0, 1]}], "nodes": nodes,
        "skins": [{"name": "rig", "joints": list(range(1, 1 + len(joints))), "inverseBindMatrices": ibm_acc, "skeleton": 1}],
        "meshes": [{"name": "rig_figure", "primitives": primitives}], "materials": materials, "animations": animations,
        "buffers": [{"byteLength": len(blob)}], "bufferViews": views, "accessors": accessors,
    }
    js = json.dumps(gltf, separators=(",", ":")).encode("utf-8")
    js += b" " * ((4 - len(js) % 4) % 4)
    total = 12 + 8 + len(js) + 8 + len(blob)
    path = os.path.join(out, "rig_figure.glb")
    with open(path, "wb") as f:
        f.write(struct.pack("<III", 0x46546C67, 2, total))
        f.write(struct.pack("<II", len(js), 0x4E4F534A))
        f.write(js)
        f.write(struct.pack("<II", len(blob), 0x004E4942))
        f.write(blob)
    print("wrote %s (%d triangles, %d joints, %d clips, %d bytes)" % (os.path.relpath(path, ROOT), tris, len(joints), len(clips), total))


if __name__ == "__main__":
    os.makedirs(OUT_DIR, exist_ok=True)
    make_tree()
    make_rock()
    make_ring()
    make_crate()
    make_fence()
    make_styles()
    make_probe_ball()
    make_rig()

# Converts .fbx files to .glb with Blender, without a window (the FBX path, docs "Blender add-on"):
#   blender -b --factory-startup --python tools/fbx_to_glb.py -- <input.fbx> [more.fbx ...] --out <folder>
# Each <name>.fbx becomes <folder>/<name>.glb. FBX files made in Mixamo, Cascadeur or 3ds Max keep their skeleton and animation clips.
# Use tools/fbx_to_glb.ps1 to find Blender and handle folders.
import os
import sys

import bpy


def main():
    args = sys.argv[sys.argv.index("--") + 1:]
    if "--out" not in args:
        sys.exit("usage: -- <input.fbx> [...] --out <folder>")
    i = args.index("--out")
    out = os.path.abspath(args[i + 1])
    inputs = args[:i] + args[i + 2:]
    os.makedirs(out, exist_ok=True)
    failed = 0
    for src in inputs:
        name = os.path.splitext(os.path.basename(src))[0]
        dst = os.path.join(out, name + ".glb")
        try:
            bpy.ops.wm.read_factory_settings(use_empty=True)
            bpy.ops.import_scene.fbx(filepath=os.path.abspath(src))
            bpy.ops.export_scene.gltf(filepath=dst, export_format="GLB", export_animations=True,
                                      export_apply=False, export_yup=True)
            print("converted %s -> %s" % (src, dst))
        except Exception as e:  # noqa: BLE001 (report and go on with the next file)
            failed += 1
            print("FAILED %s: %s" % (src, e))
    sys.exit(1 if failed else 0)


main()

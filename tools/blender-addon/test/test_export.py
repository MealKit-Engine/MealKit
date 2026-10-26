# 애드온 자동 시험 (check.ps1 "Blender add-on export"): Blender를 창 없이 돌려
#   blender -b --factory-startup --python tools/blender-addon/test/test_export.py -- <임시 폴더>
# 임시 프로젝트(mealkit.project + art/test.blend)에 텍스처 재질 상자와 정점 색 구를 만들고, 검사 경고와 내보낸 .glb를 확인한다.
import json
import os
import struct
import sys

import bpy

here = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(here))
import mealkit  # noqa: E402

failures = []


def expect(ok, what):
    print(("ok   " if ok else "FAIL ") + what)
    if not ok:
        failures.append(what)


def main():
    root = os.path.abspath(sys.argv[sys.argv.index("--") + 1])
    os.makedirs(os.path.join(root, "art"), exist_ok=True)
    with open(os.path.join(root, "mealkit.project"), "w", encoding="utf-8") as f:
        f.write("name addontest\n")

    mealkit.register()
    bpy.ops.object.select_all(action="SELECT")  # 기본 장면(상자·카메라·빛)을 비운다
    bpy.ops.object.delete()

    # 텍스처를 쓰는 재질의 상자 (경고), 정점 색을 기본 색으로 쓰는 구 (지원)
    bpy.ops.mesh.primitive_cube_add(size=2)
    cube = bpy.context.active_object
    textured = bpy.data.materials.new("Textured")
    tex = textured.node_tree.nodes.new("ShaderNodeTexImage")
    tex.image = bpy.data.images.new("checker", 64, 64, alpha=False)
    tex.image.generated_type = "COLOR_GRID"
    tex.image.pack()
    bsdf = textured.node_tree.nodes["Principled BSDF"]
    textured.node_tree.links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])
    cube.data.materials.append(textured)

    bpy.ops.mesh.primitive_uv_sphere_add(radius=1, location=(3, 0, 0))
    sphere = bpy.context.active_object
    colors = sphere.data.color_attributes.new("Col", "BYTE_COLOR", "CORNER")
    for c in colors.data:
        c.color = (1.0, 0.2, 0.1, 1.0)
    painted = bpy.data.materials.new("Painted")
    attr = painted.node_tree.nodes.new("ShaderNodeVertexColor")
    attr.layer_name = "Col"
    painted.node_tree.links.new(attr.outputs["Color"], painted.node_tree.nodes["Principled BSDF"].inputs["Base Color"])
    sphere.data.materials.append(painted)

    # 스타일 프리셋 머티리얼 기능: 패널의 Add 단추(연산자)가 커스텀 속성 = glTF extras를 만든다. 바람 세기는 정점 알파
    for c in colors.data:
        c.color = (1.0, 0.2, 0.1, 0.5)
    features = {f["key"]: f for f in mealkit.schema()["materialFeatures"]}
    sphere.active_material = painted
    bpy.context.view_layer.objects.active = sphere
    for key in ("outline", "outlineColor", "wind"):
        with bpy.context.temp_override(material=painted):
            bpy.ops.mealkit.feature_add(key="mealkit_" + key, value=json.dumps(features[key]["default"]),
                                        color=features[key]["color"])
    painted["mealkit_outline"] = 0.04
    expect(painted.id_properties_ui("mealkit_outlineColor").as_dict().get("subtype") == "COLOR", "color feature uses a color picker")

    blend =os.path.join(root, "art", "props.blend")
    bpy.ops.wm.save_as_mainfile(filepath=blend)

    expect(mealkit.find_project(blend) == root, "project found from art/props.blend")
    expect(mealkit.target_path(bpy.context) == os.path.join(root, "assets", "models", "props.glb"),
           "target is assets/models/props.glb")

    bpy.ops.mealkit.check()
    texts = [(p.level, p.text) for p in bpy.context.scene.mealkit.problems]
    for t in texts:
        print("     ", t)
    expect(not any("Textured" in t for _, t in texts), "base color image texture is supported")
    expect(not any("Painted" in t for _, t in texts), "vertex color material is supported")
    expect(not any(l == "ERROR" for l, _ in texts), "no errors")

    result = bpy.ops.mealkit.export()
    path = os.path.join(root, "assets", "models", "props.glb")
    expect(result == {"FINISHED"} and os.path.isfile(path), "exported props.glb")
    if os.path.isfile(path):
        with open(path, "rb") as f:
            data = f.read()
        magic, _, _ = struct.unpack_from("<4sII", data, 0)
        length, kind = struct.unpack_from("<I4s", data, 12)
        gltf = json.loads(data[20:20 + length])
        prims = [p for m in gltf.get("meshes", []) for p in m["primitives"]]
        expect(magic == b"glTF" and kind == b"JSON", "binary glTF")
        expect(len(prims) == 2, "two primitives")
        expect(any("COLOR_0" in p["attributes"] for p in prims), "vertex colors exported as COLOR_0")
        expect(any("TEXCOORD_0" in p["attributes"] for p in prims), "texture coordinates exported")
        expect(len(gltf.get("images", [])) == 1, "the texture image is in the .glb")
        mats = {m.get("name"): m for m in gltf.get("materials", [])}
        extras = mats.get("Painted", {}).get("extras", {})
        expect(extras.get("mealkit_outline") == 0.04 and len(extras.get("mealkit_outlineColor", [])) == 3
               and "mealkit_wind" in extras, "material features exported as extras")
        accessors = gltf.get("accessors", [])
        colored = [p for p in prims if "COLOR_0" in p["attributes"]]
        expect(bool(colored) and accessors[colored[0]["attributes"]["COLOR_0"]]["type"] == "VEC4",
               "vertex color alpha (wind amount) exported")

    # 에셋 예산: 이름의 첫 폴더가 종류, 한도는 mealkit.project (소품 삼각형 500으로 낮춘다)
    with open(os.path.join(root, "mealkit.project"), "a", encoding="utf-8") as f:
        f.write("budgetPropsTriangles 500\n")
    bpy.context.scene.mealkit.model_name = "props/heavy"
    expect(mealkit.model_kind("props/heavy")["folder"] == "props" and mealkit.model_kind("characters/hero")["folder"] == "characters"
           and mealkit.model_kind("crate")["folder"] == "props", "budget kind from the folder")
    bpy.ops.mealkit.check()
    texts = [p.text for p in bpy.context.scene.mealkit.problems]
    expect(any("over the props budget (500)" in t for t in texts), "triangle budget warning from mealkit.project")
    bpy.ops.mealkit.export()
    expect(os.path.isfile(os.path.join(root, "assets", "models", "props", "heavy.glb")), "exported into models/props/")

    # 아무것도 없으면 내보내지 않는다
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete()
    try:  # 오류 보고가 있는 연산자는 파이썬 호출에서 예외가 된다
        refused = bpy.ops.mealkit.export() == {"CANCELLED"}
    except RuntimeError as e:
        refused = "Nothing to export" in str(e)
    expect(refused, "empty scene is not exported")

    mealkit.unregister()


try:
    main()
except Exception as e:  # 시험 스크립트 오류도 실패로
    import traceback
    traceback.print_exc()
    failures.append(str(e))
print("BLENDER ADDON " + ("FAIL" if failures else "ok"))
sys.stdout.flush()
os._exit(1 if failures else 0)

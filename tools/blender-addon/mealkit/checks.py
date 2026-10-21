# 내보내기 전 검사: 엔진이 읽지 않는 것을 알려 준다 (schema.json의 model). 결과는 (수준, 글자) 목록.
#   ERROR   내보내지 않는다
#   WARNING 내보내지만 엔진에서 다르게 보인다
#   INFO    참고 (삼각형 수 등)

# 기본 색이 이 노드에서 오면 엔진이 정점 색(COLOR_0)으로 읽는다
VERTEX_COLOR_NODES = {"VERTEX_COLOR", "ATTRIBUTE"}


def _principled(material):
    if not material.node_tree:  # Blender 5.x: 재질은 항상 노드 (use_nodes는 6.0에서 없어진다)
        return None
    for node in material.node_tree.nodes:
        if node.type == "BSDF_PRINCIPLED":
            return node
    return None


def check_material(material, model):
    out = []
    name = material.name
    tree = material.node_tree
    if tree and not model["material"]["textures"]:
        if any(n.type == "TEX_IMAGE" for n in tree.nodes):
            out.append(("WARNING", f"Material '{name}': image textures are not supported yet (only the base color value)"))
    bsdf = _principled(material)
    if tree and bsdf is None:
        out.append(("WARNING", f"Material '{name}': no Principled BSDF, the engine shows it white"))
    elif bsdf is not None:
        base = bsdf.inputs.get("Base Color")
        if base is not None and base.is_linked:
            source = base.links[0].from_node
            if source.type not in VERTEX_COLOR_NODES and source.type != "TEX_IMAGE":  # 텍스처는 위에서 알렸다
                out.append(("WARNING", f"Material '{name}': base color comes from a '{source.bl_label}' node; "
                                       "the engine uses only a color value or the vertex color"))
    return out


def check_objects(objects, schema, depsgraph, kind="props", budget=None):
    model = schema["model"]
    out = []
    if not objects:
        return [("ERROR", "Nothing to export: no visible (or selected) mesh objects")]
    materials = {}
    triangles = 0
    for obj in objects:
        evaluated = obj.evaluated_get(depsgraph)
        mesh = evaluated.to_mesh()
        if mesh is not None:
            mesh.calc_loop_triangles()
            triangles += len(mesh.loop_triangles)
            if model["material"]["vertexColor"] and len(mesh.color_attributes) > 1:
                out.append(("INFO", f"'{obj.name}': {len(mesh.color_attributes)} color attributes, "
                                    "only the active one is exported"))
            evaluated.to_mesh_clear()
        for slot in obj.material_slots:
            if slot.material:
                materials[slot.material.name] = slot.material
        if not model["skinning"] and (obj.find_armature() or any(m.type == "ARMATURE" for m in obj.modifiers)):
            out.append(("WARNING", f"'{obj.name}': skinning is not supported yet, exported in its current pose"))
        if not model["animation"] and obj.animation_data and obj.animation_data.action:
            out.append(("WARNING", f"'{obj.name}': animation is not supported yet and is not exported"))
        if obj.data is not None and getattr(obj.data, "shape_keys", None):
            out.append(("WARNING", f"'{obj.name}': shape keys are not supported, the current shape is exported"))
    for material in materials.values():
        out += check_material(material, model)
    if model.get("mergesMeshes"):
        out.append(("INFO", f"{len(objects)} object(s) become one model, {triangles} triangles"))
    # 에셋 예산 (mealkit.project budget*, 종류는 폴더). 엔진도 불러올 때 같은 값으로 경고한다
    if budget:
        if triangles > budget["triangles"]:
            out.append(("WARNING", f"{triangles} triangles is over the {kind} budget ({budget['triangles']})"))
        used = len(materials) or 1  # 재질 없는 메시는 기본 재질 하나
        if used > budget["materials"]:
            out.append(("WARNING", f"{used} materials is over the {kind} budget ({budget['materials']})"))
    return out

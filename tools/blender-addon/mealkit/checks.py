# Pre-export checks: reports what the engine doesn't read (model in schema.json). Results are a list of (level, text).
#   ERROR   not exported
#   WARNING exported but looks different in the engine
#   INFO    for reference (triangle count etc.)

# If the base color comes from this node, the engine reads it as vertex color (COLOR_0)
VERTEX_COLOR_NODES = {"VERTEX_COLOR", "ATTRIBUTE"}


def _principled(material):
    if not material.node_tree:  # Blender 5.x: materials always have nodes (use_nodes goes away in 6.0)
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
    if tree and model["material"]["textures"]:
        # Images connected outside the texture inputs the engine reads (Principled BSDF inputs exported to glTF) aren't used
        bsdf_inputs = set(model["material"].get("textureInputs", []))
        bsdf = _principled(material)
        for inp in (bsdf.inputs if bsdf is not None else []):
            if inp.is_linked and inp.name not in bsdf_inputs and inp.name not in ("Alpha",) and inp.links[0].from_node.type == "TEX_IMAGE":
                out.append(("WARNING", f"Material '{name}': a texture on '{inp.name}' is not used by the engine"))
    bsdf = _principled(material)
    if tree and bsdf is None:
        out.append(("WARNING", f"Material '{name}': no Principled BSDF, the engine shows it white"))
    elif bsdf is not None:
        base = bsdf.inputs.get("Base Color")
        if base is not None and base.is_linked:
            source = base.links[0].from_node
            if source.type not in VERTEX_COLOR_NODES and source.type != "TEX_IMAGE":  # images/vertex colors are read by the engine
                out.append(("WARNING", f"Material '{name}': base color comes from a '{source.bl_label}' node; "
                                       "the engine uses a color value, an image texture or the vertex color"))
    return out


def check_objects(objects, schema, depsgraph, kind="props", budget=None):
    model = schema["model"]
    out = []
    if not objects:
        return [("ERROR", "Nothing to export: no visible (or selected) mesh objects")]
    materials = {}
    triangles = 0
    bones = 0
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
        armature = obj.find_armature()
        if model["skinning"] and armature is not None:
            # The engine supports one skin with up to maxJoints joints (beyond that it draws the bind pose). The budget's bones are counted here too
            joints = len(armature.data.bones)
            bones = max(bones, joints)
            if joints > model.get("maxJoints", 64):
                out.append(("ERROR", f"'{armature.name}': {joints} bones, the engine supports {model.get('maxJoints', 64)}"))
        if not model["skinning"] and (armature or any(m.type == "ARMATURE" for m in obj.modifiers)):
            out.append(("WARNING", f"'{obj.name}': skinning is not supported yet, exported in its current pose"))
        if not model["animation"] and obj.animation_data and obj.animation_data.action:
            out.append(("WARNING", f"'{obj.name}': animation is not supported yet and is not exported"))
        if obj.data is not None and getattr(obj.data, "shape_keys", None):
            out.append(("WARNING", f"'{obj.name}': shape keys are not supported, the current shape is exported"))
    for material in materials.values():
        out += check_material(material, model)
    if model.get("mergesMeshes"):
        out.append(("INFO", f"{len(objects)} object(s) become one model, {triangles} triangles"))
    # Asset budget (mealkit.project budget*, kind by folder). The engine also warns on load with the same values
    if budget:
        if triangles > budget["triangles"]:
            out.append(("WARNING", f"{triangles} triangles is over the {kind} budget ({budget['triangles']})"))
        used = len(materials) or 1  # meshes without materials use one default material
        if used > budget["materials"]:
            out.append(("WARNING", f"{used} materials is over the {kind} budget ({budget['materials']})"))
        if bones > budget["bones"]:
            out.append(("WARNING", f"{bones} bones is over the {kind} budget ({budget['bones']})"))
    return out

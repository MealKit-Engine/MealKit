# MealKit Blender 애드온 (docs/ROADMAP.md 2단계 "Blender 내보내기 애드온", Penpot 플러그인과 같은 역할).
# - 엔진의 지원 범위 스키마(schema.json, 엔진 asset::modelSchemaJson과 같은지 단위 테스트가 본다)로 장면을 검사한다
# - 버튼 하나로 프로젝트 assets/models/<이름>.glb에 정해진 설정으로 내보낸다 (에디터 에셋 핫 리로드가 받아 바로 보인다)
# - 머티리얼 패널에 엔진 머티리얼 기능(스키마의 materialFeatures)을 보여 주고 커스텀 속성 = glTF extras로 쓴다
# Blender 쪽 글자는 영어 (Blender UI 언어와 같이 두고, 한국어 설명은 문서 저장소)
import json
import os

import bpy

from . import checks

SCHEMA = None


def schema():
    global SCHEMA
    if SCHEMA is None:
        with open(os.path.join(os.path.dirname(__file__), "schema.json"), encoding="utf-8") as f:
            SCHEMA = json.load(f)
    return SCHEMA


def find_project(start):
    """.blend 파일 폴더에서 위로 올라가며 mealkit.project가 있는 폴더 (원본은 보통 <프로젝트>/art/)."""
    folder = os.path.dirname(os.path.abspath(start)) if start else ""
    while folder:
        if os.path.isfile(os.path.join(folder, "mealkit.project")):
            return folder
        parent = os.path.dirname(folder)
        if parent == folder:
            break
        folder = parent
    return ""


def project_folder(context):
    override = context.scene.mealkit.project
    if override:
        return bpy.path.abspath(override)
    return find_project(bpy.data.filepath)


def model_name(context):
    name = context.scene.mealkit.model_name.strip()
    if not name and bpy.data.filepath:
        name = os.path.splitext(os.path.basename(bpy.data.filepath))[0]
    return name


def target_path(context):
    project = project_folder(context)
    name = model_name(context)
    if not project or not name:
        return ""
    model = schema()["model"]
    return os.path.join(project, "assets", model["folder"], name + "." + model["format"])


def export_objects(context):
    use_selection = context.scene.mealkit.selection_only
    objects = context.selected_objects if use_selection else context.visible_objects
    return [o for o in objects if o.type in {"MESH", "CURVE", "SURFACE", "META", "FONT"}]


def run_checks(context):
    problems = []
    if not project_folder(context):
        problems.append(("ERROR", "No MealKit project: save the .blend inside a project folder, or set the project"))
    if not model_name(context):
        problems.append(("ERROR", "No model name: save the .blend or type a name"))
    problems += checks.check_objects(export_objects(context), schema(), context.evaluated_depsgraph_get())
    return problems


class MealKitProblem(bpy.types.PropertyGroup):
    level: bpy.props.StringProperty()
    text: bpy.props.StringProperty()


class MealKitSceneSettings(bpy.types.PropertyGroup):
    project: bpy.props.StringProperty(name="Project", subtype="DIR_PATH",
                                      description="MealKit project folder (empty: found from the .blend location)")
    model_name: bpy.props.StringProperty(name="Name", description="File name under assets/models (empty: the .blend name)")
    selection_only: bpy.props.BoolProperty(name="Selected only", default=False,
                                           description="Export only the selected objects (otherwise all visible objects)")
    problems: bpy.props.CollectionProperty(type=MealKitProblem)
    checked: bpy.props.BoolProperty(default=False)


def store(context, problems):
    settings = context.scene.mealkit
    settings.problems.clear()
    for level, text in problems:
        item = settings.problems.add()
        item.level = level
        item.text = text
    settings.checked = True


class MEALKIT_OT_check(bpy.types.Operator):
    bl_idname = "mealkit.check"
    bl_label = "Check"
    bl_description = "Check the objects against what the MealKit engine supports"

    def execute(self, context):
        problems = run_checks(context)
        store(context, problems)
        for level, text in problems:
            print(f"MealKit {level}: {text}")
        return {"FINISHED"}


class MEALKIT_OT_export(bpy.types.Operator):
    bl_idname = "mealkit.export"
    bl_label = "Export to MealKit"
    bl_description = "Check, then export a .glb into the project's assets/models with the engine's settings"

    def execute(self, context):
        problems = run_checks(context)
        store(context, problems)
        errors = [t for level, t in problems if level == "ERROR"]
        if errors:
            self.report({"ERROR"}, errors[0])
            return {"CANCELLED"}
        path = target_path(context)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        model = schema()["model"]
        # 엔진이 읽는 것만: 위치·법선·정점 색(COLOR_0)·재질 기본 색. 텍스처·애니메이션·스킨은 엔진이 아직 읽지 않는다
        bpy.ops.export_scene.gltf(
            filepath=path,
            export_format="GLB",
            use_selection=context.scene.mealkit.selection_only,
            use_visible=not context.scene.mealkit.selection_only,
            export_apply=True,
            export_yup=True,
            export_normals=True,
            export_texcoords=model["material"]["textures"],
            export_tangents=False,
            export_vertex_color="ACTIVE" if model["material"]["vertexColor"] else "NONE",
            export_materials="EXPORT",
            export_image_format="AUTO" if model["material"]["textures"] else "NONE",
            export_animations=model["animation"],
            export_skins=model["skinning"],
            export_morph=False,
            export_cameras=False,
            export_lights=False,
            export_extras=True,
        )
        warnings = sum(1 for level, _ in problems if level == "WARNING")
        message = f"Exported {os.path.relpath(path, project_folder(context))}"
        if warnings:
            message += f" ({warnings} warning(s), see the MealKit panel)"
        self.report({"WARNING"} if warnings else {"INFO"}, message)
        print("MealKit export:", path)
        return {"FINISHED"}


class MEALKIT_PT_export(bpy.types.Panel):
    bl_label = "MealKit"
    bl_space_type = "VIEW_3D"
    bl_region_type = "UI"
    bl_category = "MealKit"

    def draw(self, context):
        layout = self.layout
        settings = context.scene.mealkit
        project = project_folder(context)
        layout.label(text=f"Project: {project or 'not found'}", icon="FILE_FOLDER" if project else "ERROR")
        layout.prop(settings, "project")
        layout.prop(settings, "model_name", placeholder=model_name(context) or "model")
        layout.prop(settings, "selection_only")
        path = target_path(context)
        if path:
            layout.label(text=os.path.relpath(path, project), icon="EXPORT")
        row = layout.row()
        row.operator("mealkit.check", icon="CHECKMARK")
        row.operator("mealkit.export", icon="EXPORT")
        if settings.checked:
            box = layout.box()
            if not settings.problems:
                box.label(text="Everything is supported", icon="CHECKMARK")
            for p in settings.problems:
                icon = {"ERROR": "ERROR", "WARNING": "INFO"}.get(p.level, "DOT")
                box.label(text=p.text, icon=icon)


class MEALKIT_PT_material(bpy.types.Panel):
    bl_label = "MealKit"
    bl_space_type = "PROPERTIES"
    bl_region_type = "WINDOW"
    bl_context = "material"

    @classmethod
    def poll(cls, context):
        return context.material is not None

    def draw(self, context):
        layout = self.layout
        material = context.material
        features = schema()["materialFeatures"]
        if not features:
            layout.label(text="No engine material features yet: base color and vertex color are used")
            return
        # 기능마다 커스텀 속성 "mealkit_<키>" (glTF extras로 나간다). 스키마가 종류·기본값을 준다
        for feature in features:
            key = "mealkit_" + feature["key"]
            row = layout.row()
            if key in material:
                row.prop(material, f'["{key}"]', text=feature.get("label", feature["key"]))
                row.operator("mealkit.feature_remove", text="", icon="X").key = key
            else:
                op = row.operator("mealkit.feature_add", text="Add " + feature.get("label", feature["key"]))
                op.key = key
                op.value = json.dumps(feature.get("default", 0))


class MEALKIT_OT_feature_add(bpy.types.Operator):
    bl_idname = "mealkit.feature_add"
    bl_label = "Add MealKit feature"
    key: bpy.props.StringProperty()
    value: bpy.props.StringProperty()

    def execute(self, context):
        context.material[self.key] = json.loads(self.value)
        return {"FINISHED"}


class MEALKIT_OT_feature_remove(bpy.types.Operator):
    bl_idname = "mealkit.feature_remove"
    bl_label = "Remove MealKit feature"
    key: bpy.props.StringProperty()

    def execute(self, context):
        if self.key in context.material:
            del context.material[self.key]
        return {"FINISHED"}


CLASSES = (MealKitProblem, MealKitSceneSettings, MEALKIT_OT_check, MEALKIT_OT_export, MEALKIT_OT_feature_add,
           MEALKIT_OT_feature_remove, MEALKIT_PT_export, MEALKIT_PT_material)


def register():
    for c in CLASSES:
        bpy.utils.register_class(c)
    bpy.types.Scene.mealkit = bpy.props.PointerProperty(type=MealKitSceneSettings)


def unregister():
    del bpy.types.Scene.mealkit
    for c in reversed(CLASSES):
        bpy.utils.unregister_class(c)

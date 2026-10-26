#include "mealkit/scene/Scene.h"

#include "mealkit/core/Hash.h"
#include "mealkit/core/Log.h"
#include "mealkit/core/TextFormat.h"
#include "mealkit/platform/Paths.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace mealkit::scene {
namespace {

constexpr u32 rgba(u8 r, u8 g, u8 b, u8 a = 255) {
    return static_cast<u32>(r) | (static_cast<u32>(g) << 8) | (static_cast<u32>(b) << 16) | (static_cast<u32>(a) << 24);
}

bool readFloats(const char* text, f32* out, int count) {
    const char* p = text;
    for (int i = 0; i < count; ++i) {
        char* end = nullptr;
        out[i] = std::strtof(p, &end);
        if (end == p) return false;
        p = end;
    }
    return true;
}

bool readVec3(const char* text, Vec3& v) {
    f32 f[3];
    if (!readFloats(text, f, 3)) return false;
    v = {f[0], f[1], f[2]};
    return true;
}

reflect::TypeInfo makeBuiltin(const char* name, const char* label, u32 flags) {
    reflect::TypeInfo t;
    t.name = name;
    t.label = label;
    t.flags = flags;
    t.size = 1;
    t.create = []() -> void* { return new u8(0); };
    t.destroy = [](void* p) { delete static_cast<u8*>(p); };
    t.copy = [](void*, const void*) {};
    return t;
}

// 종류 이름: 영문자로 시작하고 영문자·숫자·_ 만 (C++ 타입 이름과 같은 규칙)
bool validTypeName(const char* text) {
    if (!std::isalpha(static_cast<unsigned char>(*text))) return false;
    for (const char* p = text; *p; ++p)
        if (!std::isalnum(static_cast<unsigned char>(*p)) && *p != '_') return false;
    return true;
}

void writeVec3(FILE* f, const char* key, Vec3 v) { fprintf(f, "%s %.9g %.9g %.9g\n", key, v.x, v.y, v.z); }

} // namespace

const std::vector<const reflect::TypeInfo*>& builtinTypes() {
    static const reflect::TypeInfo solid = makeBuiltin(kTypeSolid, "Solid box", 0);
    static const reflect::TypeInfo decoration =
        makeBuiltin(kTypeDecoration, "Decoration (no collision)", reflect::kObjectCosmetic);
    static const reflect::TypeInfo spawn = makeBuiltin(kTypeSpawn, "Spawn point", reflect::kObjectMarker);
    static const reflect::TypeInfo probe =
        makeBuiltin(kTypeProbe, "Reflection probe", reflect::kObjectArea | reflect::kObjectCosmetic); // 크기 = 영향 상자
    static const reflect::TypeInfo pointLight = [] { // 필드가 있는 기본 종류: PointLightFields의 설명을 이름만 바꿔 쓴다
        reflect::TypeInfo t = reflect::typeOf<PointLightFields>();
        const reflect::TypeInfo b = makeBuiltin(kTypePointLight, "Point light", reflect::kObjectMarker | reflect::kObjectCosmetic);
        t.name = b.name;
        t.label = b.label;
        t.flags = b.flags;
        return t;
    }();
    static const std::vector<const reflect::TypeInfo*> list{&solid, &decoration, &spawn, &probe, &pointLight};
    return list;
}

std::string probeFile(const Entity& probe) {
    for (const auto& [key, value] : probe.fields)
        if (key == kProbeFileField) return value;
    return {};
}

std::string probeFileFor(const std::string& sceneName, const Entity& probe) {
    return "probes/" + (sceneName.empty() ? std::string("scene") : sceneName) + "_" + std::to_string(probe.id) + ".mkprobe";
}

const reflect::TypeInfo* findType(const std::string& name, const TypeList* gameTypes) {
    for (const reflect::TypeInfo* t : builtinTypes())
        if (name == t->name) return t;
    if (gameTypes)
        for (const reflect::TypeInfo* t : *gameTypes)
            if (name == t->name) return t;
    return nullptr;
}

bool visibleInGame(const Entity& e, const TypeList* gameTypes) {
    const reflect::TypeInfo* type = findType(e.type, gameTypes);
    return !type || !(type->flags & (reflect::kObjectMarker | reflect::kObjectArea));
}

Bounds areaBounds(const Entity& e) {
    const Vec3 half = e.scale * 0.5f;
    return {e.position - half, e.position + half};
}

reflect::TextFields defaultFields(const reflect::TypeInfo& type) {
    reflect::Instance instance(type);
    reflect::TextFields fields;
    reflect::toText(type, instance.data(), fields);
    return fields;
}

Entity& Scene::add(const std::string& type, const std::string& name) {
    Entity e;
    e.id = nextId++;
    e.type = type;
    e.name = name;
    entities.push_back(e);
    return entities.back();
}

Entity* Scene::find(u32 id) {
    const int i = indexOf(id);
    return i >= 0 ? &entities[static_cast<size_t>(i)] : nullptr;
}

const Entity* Scene::find(u32 id) const {
    const int i = indexOf(id);
    return i >= 0 ? &entities[static_cast<size_t>(i)] : nullptr;
}

int Scene::indexOf(u32 id) const {
    for (size_t i = 0; i < entities.size(); ++i)
        if (entities[i].id == id) return static_cast<int>(i);
    return -1;
}

u64 Scene::contentHash(const TypeList* gameTypes) const {
    Hasher h;
    for (const Entity& e : entities) {
        const reflect::TypeInfo* type = findType(e.type, gameTypes);
        if (type && (type->flags & reflect::kObjectCosmetic)) continue;
        h.add(e.id);
        h.add(e.parent);
        h.bytes(e.type.data(), e.type.size() + 1);
        h.add(e.position);
        h.add(e.rotation);
        h.add(e.scale);
        h.bytes(e.model.data(), e.model.size() + 1);
        for (const auto& [path, value] : e.fields) {
            if (type) { // 보기 전용 필드는 뺀다 (경로의 첫 이름이 필드 이름)
                const std::string top = path.substr(0, path.find_first_of(".["));
                const reflect::FieldInfo* f = type->field(top.c_str());
                if (f && (f->flags & reflect::kFieldView)) continue;
            }
            h.bytes(path.data(), path.size() + 1);
            h.bytes(value.data(), value.size() + 1);
        }
    }
    return h.value;
}

bool save(const Scene& scene, const std::string& path) {
    FILE* f = openFile(path, "wb");
    if (!f) {
        MEALKIT_ERROR("Cannot write scene: %s", path.c_str());
        return false;
    }
    const Environment& env = scene.environment;
    fprintf(f, "# MealKit scene\n%s\n\n", text::formatLine(text::kScene).c_str());
    writeVec3(f, "sun", env.sunDirection);
    writeVec3(f, "sunColor", env.sunColor);
    writeVec3(f, "sky", env.skyColor);
    writeVec3(f, "groundColor", env.groundColor);
    fprintf(f, "fog %.9g %.9g %.9g %.9g\n", env.fogColor.x, env.fogColor.y, env.fogColor.z, env.fogDensity);
    if (!env.hdri.empty())
        fprintf(f, "hdri %s\nhdriIntensity %.9g\nhdriRotation %.9g\n", env.hdri.c_str(), env.hdriIntensity, env.hdriRotation);
    for (const Entity& e : scene.entities) {
        fprintf(f, "\nentity\nid %u\nname %s\nkind %s\n", e.id, e.name.c_str(), e.type.c_str());
        if (e.parent) fprintf(f, "parent %u\n", e.parent);
        writeVec3(f, "position", e.position);
        fprintf(f, "rotation %.9g %.9g %.9g %.9g\n", e.rotation.x, e.rotation.y, e.rotation.z, e.rotation.w);
        writeVec3(f, "scale", e.scale);
        if (!e.model.empty()) fprintf(f, "model %s\n", e.model.c_str());
        fprintf(f, "color %08X\nflags %u\n", e.color, e.flags);
        for (const auto& [key, value] : e.fields) fprintf(f, "field %s %s\n", key.c_str(), value.c_str());
        fprintf(f, "end\n");
    }
    const bool ok = fflush(f) == 0;
    fclose(f);
    return ok;
}

bool load(Scene& out, const std::string& path, std::string* error) {
    FILE* f = openFile(path, "rb");
    if (!f) {
        if (error) *error = "cannot open file";
        return false;
    }

    Scene scene;
    Entity e;
    bool inEntity = false;
    int lineNo = 0;
    char line[1024];
    char message[160] = {};

    while (fgets(line, sizeof(line), f)) {
        ++lineNo;
        size_t len = std::strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r' || line[len - 1] == ' ')) line[--len] = '\0';
        char* p = line;
        while (*p == ' ' || *p == '\t') ++p;
        if (*p == '\0' || *p == '#') continue;
        u32 version = text::kScene.version; // 버전 1~3 모두 지금 모양 그대로 읽는다 (3은 hdri 줄을 더했을 뿐, migrate 없음)
        if (text::readFormatLine(p, text::kScene, path, version)) continue;

        char* rest = p;
        while (*rest && *rest != ' ') ++rest;
        if (*rest) *rest++ = '\0';
        const char* key = p;

        bool ok = true;
        if (std::strcmp(key, "version") == 0) { // @format 전의 형식 줄
            const int old = std::atoi(rest);
            ok = old >= 1 && old <= 2;
        } else if (std::strcmp(key, "entity") == 0) {
            ok = !inEntity;
            e = Entity{};
            inEntity = true;
        } else if (std::strcmp(key, "end") == 0) {
            ok = inEntity;
            scene.entities.push_back(e);
            inEntity = false;
        } else if (inEntity) {
            if (std::strcmp(key, "id") == 0) e.id = static_cast<u32>(std::strtoul(rest, nullptr, 10));
            else if (std::strcmp(key, "parent") == 0) e.parent = static_cast<u32>(std::strtoul(rest, nullptr, 10));
            else if (std::strcmp(key, "name") == 0) e.name = rest;
            else if (std::strcmp(key, "kind") == 0) {
                ok = validTypeName(rest);
                e.type = rest;
            } else if (std::strcmp(key, "field") == 0) {
                char* value = rest;
                while (*value && *value != ' ') ++value;
                if (*value) *value++ = '\0';
                ok = rest[0] != '\0';
                e.fields.emplace_back(rest, value);
            } else if (std::strcmp(key, "position") == 0) ok = readVec3(rest, e.position);
            else if (std::strcmp(key, "rotation") == 0) {
                f32 q[4];
                ok = readFloats(rest, q, 4);
                e.rotation = {q[0], q[1], q[2], q[3]};
            } else if (std::strcmp(key, "scale") == 0) ok = readVec3(rest, e.scale);
            else if (std::strcmp(key, "color") == 0) e.color = static_cast<u32>(std::strtoul(rest, nullptr, 16));
            else if (std::strcmp(key, "flags") == 0) e.flags = static_cast<u32>(std::strtoul(rest, nullptr, 10));
            else if (std::strcmp(key, "model") == 0) e.model = rest;
            else MEALKIT_WARN("%s:%d: unknown entity key '%s' ignored", path.c_str(), lineNo, key);
        } else {
            Environment& env = scene.environment;
            if (std::strcmp(key, "sun") == 0) ok = readVec3(rest, env.sunDirection);
            else if (std::strcmp(key, "sunColor") == 0) ok = readVec3(rest, env.sunColor);
            else if (std::strcmp(key, "sky") == 0) ok = readVec3(rest, env.skyColor);
            else if (std::strcmp(key, "groundColor") == 0) ok = readVec3(rest, env.groundColor);
            else if (std::strcmp(key, "fog") == 0) {
                f32 v[4];
                ok = readFloats(rest, v, 4);
                env.fogColor = {v[0], v[1], v[2]};
                env.fogDensity = v[3];
            } else if (std::strcmp(key, "hdri") == 0) env.hdri = rest;
            else if (std::strcmp(key, "hdriIntensity") == 0) ok = readFloats(rest, &env.hdriIntensity, 1);
            else if (std::strcmp(key, "hdriRotation") == 0) ok = readFloats(rest, &env.hdriRotation, 1);
            else MEALKIT_WARN("%s:%d: unknown key '%s' ignored", path.c_str(), lineNo, key);
        }
        if (!ok) {
            snprintf(message, sizeof(message), "line %d: invalid '%s'", lineNo, key);
            break;
        }
    }
    fclose(f);
    if (!message[0] && inEntity) snprintf(message, sizeof(message), "missing 'end' at end of file");
    if (message[0]) {
        if (error) *error = message;
        return false;
    }

    u32 maxId = 0;
    for (const Entity& ent : scene.entities) maxId = std::max(maxId, ent.id);
    for (Entity& ent : scene.entities)
        if (ent.id == 0) ent.id = ++maxId;
    scene.nextId = maxId + 1;
    // 없는 부모나 순환은 최상위로 (손으로 고친 파일 대비)
    for (Entity& ent : scene.entities) {
        if (!ent.parent) continue;
        bool bad = !scene.find(ent.parent);
        for (u32 p = ent.parent, depth = 0; !bad && p; ++depth) {
            const Entity* parent = scene.find(p);
            if (!parent || p == ent.id || depth > scene.entities.size()) bad = true;
            else p = parent->parent;
        }
        if (bad) {
            MEALKIT_WARN("%s: entity %u has an invalid parent %u - moved to the top level", path.c_str(), ent.id,
                         ent.parent);
            ent.parent = 0;
        }
    }
    out = std::move(scene);
    return true;
}

Transform localTransform(const Entity& e) { return {e.position, e.rotation, e.scale}; }

namespace {

Transform combine(const Transform& parent, const Transform& local) {
    const Vec3 scaled{local.position.x * parent.scale.x, local.position.y * parent.scale.y,
                      local.position.z * parent.scale.z};
    return {parent.position + rotate(parent.rotation, scaled),
            normalize(parent.rotation * local.rotation),
            {parent.scale.x * local.scale.x, parent.scale.y * local.scale.y, parent.scale.z * local.scale.z}};
}

Quat conjugate(Quat q) { return {-q.x, -q.y, -q.z, q.w}; }

f32 safeDiv(f32 a, f32 b) { return b > 1e-6f || b < -1e-6f ? a / b : a; }

} // namespace

Transform worldTransform(const Scene& scene, const Entity& e) {
    Transform t = localTransform(e);
    u32 parentId = e.parent;
    for (size_t depth = 0; parentId && depth <= scene.entities.size(); ++depth) {
        const Entity* parent = scene.find(parentId);
        if (!parent) break;
        t = combine(localTransform(*parent), t);
        parentId = parent->parent;
    }
    return t;
}

void setWorldTransform(const Scene& scene, Entity& e, const Transform& world) {
    const Entity* parent = e.parent ? scene.find(e.parent) : nullptr;
    if (!parent) {
        e.position = world.position;
        e.rotation = world.rotation;
        e.scale = world.scale;
        return;
    }
    const Transform p = worldTransform(scene, *parent);
    const Quat inv = conjugate(p.rotation);
    const Vec3 rel = rotate(inv, world.position - p.position);
    e.position = {safeDiv(rel.x, p.scale.x), safeDiv(rel.y, p.scale.y), safeDiv(rel.z, p.scale.z)};
    e.rotation = normalize(inv * world.rotation);
    e.scale = {safeDiv(world.scale.x, p.scale.x), safeDiv(world.scale.y, p.scale.y), safeDiv(world.scale.z, p.scale.z)};
}

bool isDescendant(const Scene& scene, u32 id, u32 ancestor) {
    for (size_t depth = 0; id && depth <= scene.entities.size(); ++depth) {
        if (id == ancestor) return true;
        const Entity* e = scene.find(id);
        if (!e) return false;
        id = e->parent;
    }
    return false;
}

std::vector<u32> subtree(const Scene& scene, u32 id) {
    std::vector<u32> ids;
    for (const Entity& e : scene.entities)
        if (isDescendant(scene, e.id, id)) ids.push_back(e.id);
    return ids;
}

Scene flattened(const Scene& scene) {
    Scene out = scene;
    for (size_t i = 0; i < scene.entities.size(); ++i) {
        const Transform world = worldTransform(scene, scene.entities[i]);
        Entity& e = out.entities[i];
        e.position = world.position;
        e.rotation = world.rotation;
        e.scale = world.scale;
        e.parent = 0;
    }
    return out;
}

Scene makeDefault() {
    Scene s;
    const Quat rotZ15{0.0f, 0.0f, 0.130526192f, 0.991444861f};
    const Quat rotY30{0.0f, 0.258819045f, 0.0f, 0.965925826f};
    const Quat rotY45{0.0f, 0.382683432f, 0.0f, 0.923879533f};

    auto box = [&](const char* kind, const char* name, Vec3 pos, Vec3 scale, u32 color, Quat rot = {}, u32 flags = 0) {
        Entity& e = s.add(kind, name);
        e.position = pos;
        e.scale = scale;
        e.color = color;
        e.rotation = rot;
        e.flags = flags;
    };

    const u32 wall = rgba(150, 146, 140);
    box(kTypeSolid, "Ground", {0, -0.5f, 0}, {120, 1, 120}, rgba(118, 150, 96), {}, 1u);
    box(kTypeSolid, "Wall North", {0, 1, 60.5f}, {122, 2, 1}, wall);
    box(kTypeSolid, "Wall South", {0, 1, -60.5f}, {122, 2, 1}, wall);
    box(kTypeSolid, "Wall East", {60.5f, 1, 0}, {1, 2, 120}, wall);
    box(kTypeSolid, "Wall West", {-60.5f, 1, 0}, {1, 2, 120}, wall);
    box(kTypeSolid, "Ramp", {18, 1.25f, -10}, {10, 0.5f, 4}, rgba(196, 160, 110), rotZ15);
    box(kTypeSolid, "Platform", {26, 2.5f, -10}, {6, 0.5f, 6}, rgba(170, 140, 100));
    box(kTypeSolid, "Pillar A", {-15, 2, 15}, {2, 4, 2}, rgba(170, 170, 182));
    box(kTypeSolid, "Pillar B", {15, 2, 15}, {2, 4, 2}, rgba(170, 170, 182));
    box(kTypeSolid, "Block", {-20, 1, -15}, {6, 2, 3}, rgba(150, 120, 200), rotY30);
    box(kTypeSolid, "Crate", {0, 1, 25}, {4, 2, 4}, rgba(205, 140, 80), rotY45);
    box(kTypeDecoration, "Banner", {0, 6, -59.9f}, {14, 5, 0.2f}, rgba(200, 60, 60));
    const u32 spawn = rgba(255, 220, 90);
    box(kTypeSpawn, "Spawn 1", {-6, 0.9f, -6}, {1, 1, 1}, spawn);
    box(kTypeSpawn, "Spawn 2", {6, 0.9f, 6}, {1, 1, 1}, spawn);
    box(kTypeSpawn, "Spawn 3", {6, 0.9f, -6}, {1, 1, 1}, spawn);
    box(kTypeSpawn, "Spawn 4", {-6, 0.9f, 6}, {1, 1, 1}, spawn);
    return s;
}

} // namespace mealkit::scene

#include "aster/scene/Scene.h"

#include "aster/core/Hash.h"
#include "aster/core/Log.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <windows.h>

namespace aster::scene {
namespace {

constexpr u32 rgba(u8 r, u8 g, u8 b, u8 a = 255) {
    return static_cast<u32>(r) | (static_cast<u32>(g) << 8) | (static_cast<u32>(b) << 16) | (static_cast<u32>(a) << 24);
}

FILE* openFile(const std::string& path, const wchar_t* mode) {
    const int len = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
    std::wstring wide(len > 0 ? len - 1 : 0, L'\0');
    if (len > 1) MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, wide.data(), len);
    FILE* f = nullptr;
    _wfopen_s(&f, wide.c_str(), mode);
    return f;
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

bool parseKind(const char* text, EntityKind& out) {
    if (std::strcmp(text, "solid") == 0) out = EntityKind::Solid;
    else if (std::strcmp(text, "decoration") == 0) out = EntityKind::Decoration;
    else if (std::strcmp(text, "spawn") == 0) out = EntityKind::SpawnPoint;
    else return false;
    return true;
}

void writeVec3(FILE* f, const char* key, Vec3 v) { fprintf(f, "%s %.9g %.9g %.9g\n", key, v.x, v.y, v.z); }

} // namespace

const char* kindName(EntityKind kind) {
    switch (kind) {
    case EntityKind::Solid: return "solid";
    case EntityKind::Decoration: return "decoration";
    case EntityKind::SpawnPoint: return "spawn";
    }
    return "solid";
}

Entity& Scene::add(EntityKind kind, const std::string& name) {
    Entity e;
    e.id = nextId++;
    e.kind = kind;
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

u64 Scene::contentHash() const {
    Hasher h;
    for (const Entity& e : entities) {
        if (e.kind == EntityKind::Decoration) continue;
        h.add(static_cast<u8>(e.kind));
        h.add(e.position);
        h.add(e.rotation);
        h.add(e.scale);
        h.bytes(e.model.data(), e.model.size());
    }
    return h.value;
}

bool save(const Scene& scene, const std::string& path) {
    FILE* f = openFile(path, L"wb");
    if (!f) {
        ASTER_ERROR("Cannot write scene: %s", path.c_str());
        return false;
    }
    const Environment& env = scene.environment;
    fprintf(f, "# Aster scene\nversion 1\n\n");
    writeVec3(f, "sun", env.sunDirection);
    writeVec3(f, "sunColor", env.sunColor);
    writeVec3(f, "sky", env.skyColor);
    writeVec3(f, "groundColor", env.groundColor);
    fprintf(f, "fog %.9g %.9g %.9g %.9g\n", env.fogColor.x, env.fogColor.y, env.fogColor.z, env.fogDensity);
    for (const Entity& e : scene.entities) {
        fprintf(f, "\nentity\nid %u\nname %s\nkind %s\n", e.id, e.name.c_str(), kindName(e.kind));
        writeVec3(f, "position", e.position);
        fprintf(f, "rotation %.9g %.9g %.9g %.9g\n", e.rotation.x, e.rotation.y, e.rotation.z, e.rotation.w);
        writeVec3(f, "scale", e.scale);
        if (!e.model.empty()) fprintf(f, "model %s\n", e.model.c_str());
        fprintf(f, "color %08X\nflags %u\nend\n", e.color, e.flags);
    }
    const bool ok = fflush(f) == 0;
    fclose(f);
    return ok;
}

bool load(Scene& out, const std::string& path, std::string* error) {
    FILE* f = openFile(path, L"rb");
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

        char* rest = p;
        while (*rest && *rest != ' ') ++rest;
        if (*rest) *rest++ = '\0';
        const char* key = p;

        bool ok = true;
        if (std::strcmp(key, "version") == 0) {
            ok = std::atoi(rest) == 1;
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
            else if (std::strcmp(key, "name") == 0) e.name = rest;
            else if (std::strcmp(key, "kind") == 0) ok = parseKind(rest, e.kind);
            else if (std::strcmp(key, "position") == 0) ok = readVec3(rest, e.position);
            else if (std::strcmp(key, "rotation") == 0) {
                f32 q[4];
                ok = readFloats(rest, q, 4);
                e.rotation = {q[0], q[1], q[2], q[3]};
            } else if (std::strcmp(key, "scale") == 0) ok = readVec3(rest, e.scale);
            else if (std::strcmp(key, "color") == 0) e.color = static_cast<u32>(std::strtoul(rest, nullptr, 16));
            else if (std::strcmp(key, "flags") == 0) e.flags = static_cast<u32>(std::strtoul(rest, nullptr, 10));
            else if (std::strcmp(key, "model") == 0) e.model = rest;
            else ASTER_WARN("%s:%d: unknown entity key '%s' ignored", path.c_str(), lineNo, key);
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
            } else ASTER_WARN("%s:%d: unknown key '%s' ignored", path.c_str(), lineNo, key);
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
    out = std::move(scene);
    return true;
}

Scene makeDefault() {
    Scene s;
    const Quat rotZ15{0.0f, 0.0f, 0.130526192f, 0.991444861f};
    const Quat rotY30{0.0f, 0.258819045f, 0.0f, 0.965925826f};
    const Quat rotY45{0.0f, 0.382683432f, 0.0f, 0.923879533f};

    auto box = [&](EntityKind kind, const char* name, Vec3 pos, Vec3 scale, u32 color, Quat rot = {}, u32 flags = 0) {
        Entity& e = s.add(kind, name);
        e.position = pos;
        e.scale = scale;
        e.color = color;
        e.rotation = rot;
        e.flags = flags;
    };

    const u32 wall = rgba(150, 146, 140);
    box(EntityKind::Solid, "Ground", {0, -0.5f, 0}, {120, 1, 120}, rgba(118, 150, 96), {}, 1u);
    box(EntityKind::Solid, "Wall North", {0, 1, 60.5f}, {122, 2, 1}, wall);
    box(EntityKind::Solid, "Wall South", {0, 1, -60.5f}, {122, 2, 1}, wall);
    box(EntityKind::Solid, "Wall East", {60.5f, 1, 0}, {1, 2, 120}, wall);
    box(EntityKind::Solid, "Wall West", {-60.5f, 1, 0}, {1, 2, 120}, wall);
    box(EntityKind::Solid, "Ramp", {18, 1.25f, -10}, {10, 0.5f, 4}, rgba(196, 160, 110), rotZ15);
    box(EntityKind::Solid, "Platform", {26, 2.5f, -10}, {6, 0.5f, 6}, rgba(170, 140, 100));
    box(EntityKind::Solid, "Pillar A", {-15, 2, 15}, {2, 4, 2}, rgba(170, 170, 182));
    box(EntityKind::Solid, "Pillar B", {15, 2, 15}, {2, 4, 2}, rgba(170, 170, 182));
    box(EntityKind::Solid, "Block", {-20, 1, -15}, {6, 2, 3}, rgba(150, 120, 200), rotY30);
    box(EntityKind::Solid, "Crate", {0, 1, 25}, {4, 2, 4}, rgba(205, 140, 80), rotY45);
    box(EntityKind::Decoration, "Banner", {0, 6, -59.9f}, {14, 5, 0.2f}, rgba(200, 60, 60));
    const u32 spawn = rgba(255, 220, 90);
    box(EntityKind::SpawnPoint, "Spawn 1", {-6, 0.9f, -6}, {1, 1, 1}, spawn);
    box(EntityKind::SpawnPoint, "Spawn 2", {6, 0.9f, 6}, {1, 1, 1}, spawn);
    box(EntityKind::SpawnPoint, "Spawn 3", {6, 0.9f, -6}, {1, 1, 1}, spawn);
    box(EntityKind::SpawnPoint, "Spawn 4", {-6, 0.9f, 6}, {1, 1, 1}, spawn);
    return s;
}

} // namespace aster::scene

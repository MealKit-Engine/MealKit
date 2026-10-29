#pragma once

#include "mealkit/core/Math.h"
#include "mealkit/core/Reflect.h"

#include <string>
#include <vector>

// Scene: list of objects placed in the editor and loaded by the game, plus environment settings.
// The file is human-readable text (.scene). Floats are saved in the shortest representation that reads back to the same float (text::writeFloats), preserved bit for bit.
// In multiplayer all peers must use the same scene file, so contentHash() is checked on connect.
//
// Object kinds (type): engine built-in kinds (solid, decoration, spawn) and kinds the game declared with MEALKIT_OBJECT.
// Field values of game kinds are held as text, so values in the scene file aren't lost even without game code or when fields change.
// The game reads them with readObject<T>(entity) when a match starts.
namespace mealkit::scene {

inline constexpr const char* kTypeSolid = "solid";           // visible, colliding box/model
inline constexpr const char* kTypeDecoration = "decoration"; // visible-only box/model
inline constexpr const char* kTypeSpawn = "spawn";           // player start position (invisible in the game)
inline constexpr const char* kTypeProbe = "reflection_probe"; // reflection probe (view only, size = influence box, invisible in the game, baked by the editor)
inline constexpr const char* kProbeFileField = "bake";        // a reflection probe's baked file (relative to assets, stored as field text)
inline constexpr const char* kAnimField = "anim";             // looping view clip of a skinned model "<clip> [seconds offset]" (on any kind, excluded from the content hash)
inline constexpr const char* kTypePointLight = "point_light"; // point light (view only, only the light is visible in the game, fields are PointLightFields)

// Fields of point light objects (a built-in kind, but edited with inspector/field lines like game kinds). Read with readObject<PointLightFields>(e)
MEALKIT_STRUCT(PointLightFields) {
    MEALKIT_FIELD(Color, color, (Color::rgb(255, 214, 160)), "Light color");
    MEALKIT_FIELD(f32, intensity, 3.0f, "Intensity");
    MEALKIT_FIELD(f32, range, 6.0f, "Range (m)");
};

// Descriptions of engine built-in kinds (no fields, only labels and flags)
const std::vector<const reflect::TypeInfo*>& builtinTypes();

struct Entity {
    u32 id = 0;
    u32 parent = 0; // parent object id (0 = top level). Position/rotation/scale are parent-relative
    std::string name;
    std::string type = kTypeSolid;
    Vec3 position;
    Quat rotation;
    Vec3 scale{1.0f, 1.0f, 1.0f};
    u32 color = 0xFFC8C8C8;     // rgba8 sRGB (same layout as render::rgba). Multiplied with the model's material color
    u32 flags = 0;              // render::kInstance* flags
    std::string model;          // glTF path (relative to the assets folder, e.g. models/tree.glb). A box if empty
    reflect::TextFields fields; // fields of game kinds (path, value text)

    bool is(const char* typeName) const { return type == typeName; }
    template <class T>
    bool is() const {
        return type == reflect::typeOf<T>().name;
    }
};

// Post-processing values (moved into render::PostParams, editor [Environment] panel). View only, so not in the content hash.
// In scene files as "post <field> <value>" lines (all of them, even when equal to defaults)
MEALKIT_STRUCT(PostFields) {
    MEALKIT_FIELD(bool, toneMapping, true, "Tone mapping (AgX)");
    MEALKIT_FIELD(f32, exposure, 0.0f, "Exposure (EV)");
    MEALKIT_FIELD(std::string, lut, {}, "Color LUT (.cube)");
    MEALKIT_FIELD(f32, bloomIntensity, 0.25f, "Bloom intensity");
    MEALKIT_FIELD(f32, bloomThreshold, 1.0f, "Bloom threshold");
    MEALKIT_FIELD(Vec3, bloomTint, (Vec3{1.0f, 1.0f, 1.0f}), "Bloom tint");
    MEALKIT_FIELD(f32, vignette, 0.0f, "Vignette");
    MEALKIT_FIELD(f32, vignetteSmoothness, 0.5f, "Vignette start");
    MEALKIT_FIELD(f32, grain, 0.0f, "Film grain");
    MEALKIT_FIELD(f32, chromaticAberration, 0.0f, "Chromatic aberration");
};

struct Environment {
    Vec3 sunDirection{0.4f, 0.8f, 0.3f};
    Vec3 sunColor{1.0f, 0.95f, 0.85f};
    Vec3 skyColor{0.30f, 0.38f, 0.50f};
    Vec3 groundColor{0.20f, 0.17f, 0.14f};
    Vec3 fogColor{0.50f, 0.62f, 0.78f};
    f32 fogDensity = 0.008f;
    // Sky HDRI (.hdr relative to assets; if empty, the sky/ground color gradient above and the fog color background). If present, skybox/ambient/reflections come from it.
    // The sun stays as sunDirection/sunColor above (match it to the HDRI's sun). View only, so not in the content hash
    std::string hdri;
    f32 hdriIntensity = 1.0f; // sky/ambient/reflection intensity
    f32 hdriRotation = 0.0f;  // y-axis rotation (degrees)
    PostFields post;
};

using TypeList = std::vector<const reflect::TypeInfo*>;

struct Scene {
    std::vector<Entity> entities;
    Environment environment;
    u32 nextId = 1;

    Entity& add(const std::string& type, const std::string& name);
    Entity* find(u32 id);
    const Entity* find(u32 id) const;
    int indexOf(u32 id) const; // -1 if missing

    // Hash of data that affects game results: decoration kinds (kObjectCosmetic), view-only fields (kFieldView), and colors are excluded.
    // gameTypes: kinds declared by the game (if absent, all fields of game kinds are included).
    u64 contentHash(const TypeList* gameTypes = nullptr) const;
};

// ---------------------------------------------------------------- Hierarchy (parent/child)
// Handled as a tree only in the editor. The game flattens it to world coordinates with flattened() on load and uses a flat array (keeps speed and determinism).
// Scale multiplies per axis (skew from non-uniform scale of a rotated parent is dropped).

struct Transform {
    Vec3 position;
    Quat rotation;
    Vec3 scale{1.0f, 1.0f, 1.0f};
};

Transform localTransform(const Entity& e);
Transform worldTransform(const Scene& scene, const Entity& e);
// Given a world transform, converts it to parent-relative values
void setWorldTransform(const Scene& scene, Entity& e, const Transform& world);
// Whether id is a descendant of ancestor (including itself)
bool isDescendant(const Scene& scene, u32 id, u32 ancestor);
// ids of id and its descendants (entities order)
std::vector<u32> subtree(const Scene& scene, u32 id);
// A copy with all objects flattened to world coordinates (parent = 0, order kept). Used by the game when a match starts.
Scene flattened(const Scene& scene);

// Finds a description by kind name (built-in kinds → gameTypes order). nullptr if missing.
const reflect::TypeInfo* findType(const std::string& name, const TypeList* gameTypes);
// Whether an object is drawn on the game screen: markers (kObjectMarker: spawn points) and areas (kObjectArea) aren't drawn
bool visibleInGame(const Entity& e, const TypeList* gameTypes);
// World coordinate box of an area object (kObjectArea) (rotation ignored: axis-aligned, objects of a flattened() scene)
struct Bounds {
    Vec3 min, max;
};
Bounds areaBounds(const Entity& e);

// Baked file path of a reflection probe (relative to assets, empty string before baking) / path decided by baking probes/<scene name>_<object id>.mkprobe
std::string probeFile(const Entity& probe);
std::string probeFileFor(const std::string& sceneName, const Entity& probe);
// Looping view clip field (kAnimField, empty string if absent)
std::string animField(const Entity& e);

// Reads the fields of a game kind. Fields not in the scene get the declaration's defaults.
template <class T>
T readObject(const Entity& e) {
    T value{};
    std::string warnings;
    reflect::fromText(reflect::typeOf<T>(), &value, e.fields, &warnings);
    return value;
}

// Field list filled with defaults (when the editor creates a new object)
reflect::TextFields defaultFields(const reflect::TypeInfo& type);

bool save(const Scene& scene, const std::string& path);
bool load(Scene& scene, const std::string& path, std::string* error = nullptr);

// Default arena: floor, walls, ramp, pillars, 4 spawn points. Rotations are constants without trig, so the same on any PC.
Scene makeDefault();

} // namespace mealkit::scene

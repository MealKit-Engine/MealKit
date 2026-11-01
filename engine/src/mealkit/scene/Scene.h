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
inline constexpr const char* kTypeVirtualCamera = "virtual_camera"; // virtual camera (view only, invisible in the game, fields are VirtualCameraFields, camera/Brain.h)

// Fields of point light objects (a built-in kind, but edited with inspector/field lines like game kinds). Read with readObject<PointLightFields>(e)
MEALKIT_STRUCT(PointLightFields) {
    MEALKIT_FIELD(Color, color, (Color::rgb(255, 214, 160)), "Light color");
    MEALKIT_FIELD(f32, intensity, 3.0f, "Intensity");
    MEALKIT_FIELD(f32, range, 6.0f, "Range (m)");
};

// Virtual camera body kinds (camera/Brain.h): how the camera position is decided from the follow target
//   ThirdPerson: spring arm behind the target (player turns it, pulls in on hitting walls) · FirstPerson: at the target's eyes
//   TopDown: fixed angle above the target · Orbit: circles the target (or its own position) by itself · Rail: closest point of a path to the target
//   Fixed: its own placed position · Free: spectator camera moved by the player
MEALKIT_ENUM(CameraBody, u8, ThirdPerson, FirstPerson, TopDown, Orbit, Rail, Fixed, Free);
MEALKIT_ENUM(CameraEase, u8, EaseInOut, Linear, Cut);

// Fields of virtual camera objects. The brain shows the active camera with the highest priority; game code only changes active/priority
MEALKIT_STRUCT(VirtualCameraFields) {
    MEALKIT_FIELD(CameraBody, body, CameraBody::ThirdPerson, "Body kind");
    MEALKIT_FIELD(i32, priority, 10, "Priority (highest active one is shown)");
    MEALKIT_FIELD(bool, active, true, "Active at start");
    MEALKIT_FIELD(std::string, follow, {}, "Follow target (object name, empty: the possessed Pawn)");
    MEALKIT_FIELD(std::string, lookAt, {}, "Look-at target (object name, empty: the follow point)");
    MEALKIT_FIELD(f32, height, 0.8f, "Follow point above the target (m)");
    MEALKIT_FIELD(f32, distance, 18.0f, "Distance (m)");
    MEALKIT_FIELD(f32, minDistance, 4.0f, "Closest zoom (m)");
    MEALKIT_FIELD(f32, maxDistance, 80.0f, "Farthest zoom (m)");
    MEALKIT_FIELD(f32, yaw, 45.0f, "Start direction (degrees)");
    MEALKIT_FIELD(f32, pitch, 30.0f, "Start height angle (degrees)");
    MEALKIT_FIELD(f32, minPitch, -6.0f, "Lowest height angle (degrees)");
    MEALKIT_FIELD(f32, maxPitch, 83.0f, "Highest height angle (degrees)");
    MEALKIT_FIELD(f32, orbitSpeed, 5.0f, "Orbit speed (degrees/s, Orbit body)");
    MEALKIT_FIELD(f32, fov, 0.0f, "Vertical field of view (degrees, 0: player setting)");
    MEALKIT_FIELD(f32, deadZone, 0.0f, "Dead zone (m, the follow point stays while the target moves inside it)");
    MEALKIT_FIELD(f32, damping, 0.0f, "Follow damping (s, 0: immediately)");
    MEALKIT_FIELD(f32, collisionRadius, 0.3f, "Wall margin of the spring arm (m)");
    MEALKIT_FIELD(f32, blendSeconds, 0.8f, "Blend time when this camera takes over (s)");
    MEALKIT_FIELD(CameraEase, ease, CameraEase::EaseInOut, "Blend easing");
    MEALKIT_FIELD(std::string, rail, {}, "Rail path (object names in order, Rail body)");
    MEALKIT_FIELD(f32, moveSpeed, 10.0f, "Move speed (m/s, Free body)");
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
// local placed under parent (parent * local)
Transform combineTransforms(const Transform& parent, const Transform& local);
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

// ---------------------------------------------------------------- Prefabs (assets/prefabs/*.prefab)
// A bundle of objects saved to a file and placed in scenes many times. Data only (kinds, transforms, field values); behavior is sim/ code.
// Placing makes plain copies (an extension of the editor's duplicate): later edits to the file don't change copies already placed,
// and the game never reads prefab files (it sees only the copies in the scene).
// The file holds entity blocks like a scene (no environment). Roots are in world coordinates relative to the first root's x/z (height kept).

// Saves roots (and their children) to path
bool savePrefab(const Scene& scene, const std::vector<u32>& roots, const std::string& path);
bool loadPrefab(Scene& prefab, const std::string& path, std::string* error = nullptr);
// Adds copies of the prefab's objects to scene with new ids, roots placed under the transform at. Returns the new root ids
std::vector<u32> placePrefab(Scene& scene, const Scene& prefab, const Transform& at);

// Default arena: floor, walls, ramp, pillars, 4 spawn points. Rotations are constants without trig, so the same on any PC.
Scene makeDefault();

} // namespace mealkit::scene

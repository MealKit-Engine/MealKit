// Vendored copy of module-scene v0.3.6; change it in that repo (tools\module.ps1 update), not here.
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
inline constexpr const char* kTypeScatter = "scatter"; // scatter brush layer (view only, fields are ScatterFields, asset/Scatter.h; the object's own transform isn't used)
inline constexpr const char* kTypeReverbZone = "reverb_zone"; // reverb area (view only, size = box, fields are ReverbZoneFields; audio::Engine::setReverbZones)
inline constexpr const char* kTypeTerrain = "terrain"; // heightmap terrain (colliding, fields are TerrainFields, asset/Terrain.h). size = the box the heights fill
inline constexpr const char* kTypeSprite = "sprite"; // image in the world (view only, fields are SpriteFields, render/Sprites.h). color = tint

inline constexpr const char* kTypePath = "path"; // ordered points an `ai` patrol follows (invisible in the game, fields are PathFields, readPath); enters the content hash
inline constexpr const char* kTypeJoint = "joint"; // physics joint between two named solids (fields are JointFields, readJoint); enters the content hash
inline constexpr const char* kTypeTilemap = "tilemap"; // painted grid of sheet cells (fields are TilemapFields); solid tiles collide as merged boxes (tilemapBoxes)

// Fields of point light objects (a built-in kind, but edited with inspector/field lines like game kinds). Read with readObject<PointLightFields>(e)
MEALKIT_STRUCT(PointLightFields) {
    MEALKIT_FIELD(Color, color, (Color::rgb(255, 214, 160)), "Light color");
    MEALKIT_FIELD(f32, intensity, 3.0f, "Intensity");
    MEALKIT_FIELD(f32, range, 6.0f, "Range (m)|gizmo=radius");
};

// Fields of reverb zones: while the listener is inside the box (the smallest one if several), buses with reverb use these values
MEALKIT_STRUCT(ReverbZoneFields) {
    MEALKIT_FIELD(f32, roomSize, 0.6f, "Room size (0..1)");
    MEALKIT_FIELD(f32, damping, 0.5f, "Damping (0..1, high sounds die faster)");
    MEALKIT_FIELD(f32, wet, 0.35f, "Reverb amount (0..1)");
    MEALKIT_FIELD(f32, width, 1.0f, "Stereo width (0..1)");
    MEALKIT_FIELD(f32, fade, 0.8f, "Change time on entering/leaving (s)");
};

// Fields of terrain objects. The heightmap is a 16-bit grayscale PNG relative to assets (square, any size from 2 samples per side):
// black = bottom of the object's box, white = top. The image's top row is the +z edge. Rotation is ignored (terrain stays axis-aligned).
// Its samples go into the content hash with the model files (asset::ModelCache::hashModels)
MEALKIT_STRUCT(TerrainFields) {
    MEALKIT_FIELD(std::string, heightmap, {}, "Heightmap (16-bit PNG relative to assets)");
};

// Fields of sprite objects: one image (or one cell of a sheet) on the object's x/y plane, facing -z with no rotation.
// Size: the object's x/y scale in meters, or, with pixelsPerUnit, the cell's pixels divided by it (times the scale).
MEALKIT_STRUCT(SpriteFields) {
    MEALKIT_FIELD(std::string, image, {}, "Image (PNG relative to assets)");
    MEALKIT_FIELD(i32, columns, 1, "Sheet columns");
    MEALKIT_FIELD(i32, rows, 1, "Sheet rows");
    MEALKIT_FIELD(i32, cell, 0, "Cell (from the top left, row by row)");
    MEALKIT_FIELD(std::string, clip, {}, "Flipbook clip (row of tables/sprite_clips.table; replaces image, columns, rows and cell)");
    MEALKIT_FIELD(f32, pivotX, 0.5f, "Pivot x (0 left .. 1 right)");
    MEALKIT_FIELD(f32, pivotY, 0.5f, "Pivot y (0 bottom .. 1 top)");
    MEALKIT_FIELD(f32, pixelsPerUnit, 0.0f, "Pixels per meter (0: size from the scale)");
    MEALKIT_FIELD(i32, layer, 0, "Layer (higher draws over lower)");
    MEALKIT_FIELD(bool, flipX, false, "Flip horizontally");
    MEALKIT_FIELD(bool, flipY, false, "Flip vertically");
    MEALKIT_FIELD(bool, pixelArt, false, "Pixel art (nearest texel, no filtering)");
};

// Fields of tilemap objects (docs/DECISIONS.md 2026-11-24 "Tilemaps are one scene object"): one grid of cells of one sheet image on the
// object's x/y plane, facing -z. The object's position is the grid's bottom left corner (z = the middle of its collision boxes), its scale z
// the thickness of the collision boxes; rotation and scale x/y are ignored (cellSize sets the size). Look fields are view only; the grid, the
// cell size and `solid` enter the content hash because solid cells collide.
// cells: one token per cell, row by row from the top row, left to right, separated by spaces: a sheet cell number (from the top left of
// the sheet, row by row) or "." for an empty cell. Missing tokens are empty. solid: sheet cell numbers that collide, "3 5 8-11".
MEALKIT_STRUCT(TilemapFields) {
    MEALKIT_VIEW_FIELD(std::string, image, {}, "Tile sheet (PNG relative to assets)");
    MEALKIT_VIEW_FIELD(i32, columns, 1, "Sheet columns");
    MEALKIT_VIEW_FIELD(i32, rows, 1, "Sheet rows");
    MEALKIT_FIELD(i32, width, 16, "Grid width (cells)");
    MEALKIT_FIELD(i32, height, 9, "Grid height (cells)");
    MEALKIT_FIELD(f32, cellSize, 1.0f, "Cell size (m)");
    MEALKIT_FIELD(std::string, solid, {}, "Solid tiles (sheet cell numbers, e.g. 3 5 8-11)");
    MEALKIT_FIELD(std::string, cells, {}, "Cells (tile numbers row by row from the top, . for empty)");
    MEALKIT_VIEW_FIELD(i32, layer, 0, "Layer (higher draws over lower)");
    MEALKIT_VIEW_FIELD(bool, pixelArt, true, "Pixel art (nearest texel, no filtering)");
};

// Fields of path objects (docs/DECISIONS.md 2026-11-25 "E1 paths"): a polyline of points in the object's own space ("x y z; x y z; ..."), so
// moving, turning or scaling the object moves the path. closed joins the last point back to the first. Read with readPath
MEALKIT_STRUCT(PathFields) {
    MEALKIT_FIELD(bool, closed, false, "Closed loop (the last point joins the first)");
    MEALKIT_FIELD(std::string, points, {}, "Points (x y z; x y z; ... in the object's space)");
};

// Fields of joint objects (docs/DECISIONS.md 2026-11-25 "E7 joints"): a physics link between two scene objects named by bodyA and bodyB (`solid`
// objects; an empty name is the world). The object's position is the anchor, its local +Y the hinge/slider axis. A solid a
// joint names turns dynamic when its dynamic flag says so (the other stays as it is). Spring and Rope run from the anchor to body B's own position.
// Hinge limits are in degrees, slider limits in meters, both from the placed pose; 0 and 0 = free. Read with readJoint
MEALKIT_ENUM(JointKind, u8, Hinge, Slider, Spring, Fixed, Rope);
MEALKIT_STRUCT(JointFields) {
    MEALKIT_FIELD(JointKind, kind, JointKind::Hinge, "Kind");
    MEALKIT_FIELD(std::string, bodyA, {}, "Body A (object name, empty = world)");
    MEALKIT_FIELD(std::string, bodyB, {}, "Body B (object name, empty = world)");
    MEALKIT_FIELD(bool, dynamicA, false, "Body A is dynamic");
    MEALKIT_FIELD(bool, dynamicB, true, "Body B is dynamic");
    MEALKIT_FIELD(f32, limitMin, 0.0f, "Lower limit (degrees for hinge, m for slider)");
    MEALKIT_FIELD(f32, limitMax, 0.0f, "Upper limit (degrees for hinge, m for slider)");
    MEALKIT_FIELD(f32, length, 0.0f, "Spring rest / rope max length (m, 0 = as placed)");
    MEALKIT_FIELD(f32, frequency, 2.0f, "Spring frequency (Hz)");
    MEALKIT_FIELD(f32, damping, 0.3f, "Spring damping ratio");
    MEALKIT_FIELD(f32, breakForce, 0.0f, "Break force (N, 0 = unbreakable)");
};

// Fields of scatter layers: one model painted many times. Instances are in file (written by the editor), not in the scene
MEALKIT_STRUCT(ScatterFields) {
    MEALKIT_FIELD(std::string, model, {}, "Model (glTF relative to assets, empty: small box)");
    MEALKIT_FIELD(std::string, file, {}, "Instance file (.scatter relative to assets)");
    MEALKIT_FIELD(f32, density, 0.5f, "Density (per m²)");
    MEALKIT_FIELD(f32, minScale, 0.8f, "Smallest size");
    MEALKIT_FIELD(f32, maxScale, 1.2f, "Largest size");
    MEALKIT_FIELD(bool, randomYaw, true, "Random rotation");
    MEALKIT_FIELD(f32, drawDistance, 120.0f, "Draw distance (m)");
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
    std::string folder;         // scene tree folder ("Props/Trees", empty = none). Editor only: not in the content hash, games don't read it
    std::string model;          // glTF path (relative to the assets folder, e.g. models/tree.glb). A box if empty
    reflect::TextFields fields; // fields of game kinds (path, value text)
    std::string source;         // "<file>:<line>" of the `entity` line when read from a file (load warnings only; not saved, not in the hash)

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
    MEALKIT_FIELD(bool, punchy, true, "Punchy look (contrast, saturation)");
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
    Vec3 skyColor{0.18f, 0.36f, 0.85f};
    Vec3 groundColor{0.20f, 0.17f, 0.14f};
    Vec3 fogColor{0.55f, 0.70f, 0.95f};
    f32 fogDensity = 0.003f;
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
// Heightmap file of a terrain object (TerrainFields::heightmap, empty string if not set) / path the editor gives new terrains: terrain/<scene name>_<object id>.png
std::string terrainFile(const Entity& terrain);
std::string scatterFileFor(const std::string& sceneName, const Entity& scatter); // terrain/<scene name>_<object id>.scatter
std::string terrainFileFor(const std::string& sceneName, const Entity& terrain);

// ---------------------------------------------------------------- Tilemaps
inline constexpr i32 kMaxTilemapCells = 1 << 20; // width * height beyond this is cut (4 MB of tile numbers)
inline constexpr i32 kNoTile = -1;
// Tile numbers of a cells text, width * height entries row by row from the top (kNoTile for "." and missing or invalid tokens)
std::vector<i32> parseTileCells(const std::string& text, i32 width, i32 height);
std::string formatTileCells(const std::vector<i32>& tiles, i32 width, i32 height);
// Whether a tile number is listed in a solid text ("3 5 8-11")
bool tileIsSolid(const std::string& solidText, i32 tile);
// Collision boxes of a tilemap object (only the object's position and scale z are used: pass objects of a flattened scene). Solid cells
// are merged: runs of a row first, then equal runs of consecutive rows, in row order. Integer merging and one multiply per value, so
// every platform gets the same boxes; deterministic, they enter the simulation like solid objects
struct TileBox {
    Vec3 center;
    Vec3 size;
};
std::vector<TileBox> tilemapBoxes(const Entity& e);

// ---------------------------------------------------------------- Paths (kind "path")
// Parses a points text ("x y z; x y z"); a piece that doesn't hold three numbers is skipped
std::vector<Vec3> parsePathPoints(const std::string& text);
std::string formatPathPoints(const std::vector<Vec3>& points);
// A path baked into world points and a length table in whole millimeters. Distances along the path are integers, so which segment a distance
// falls in is an integer comparison on every platform; the position inside a segment is one lerp (add, multiply). The only other float math is
// one sqrt per segment (correctly rounded, the same everywhere) when the table is built, and none of it uses trig, so a path is deterministic
// and may be read by the simulation (pass objects of a flattened scene: the object's own transform is applied, not its parents')
struct ScenePath {
    std::vector<Vec3> points;  // world points
    std::vector<u32> distance; // millimeters from the first point to point i; a closed path has one more entry, the whole loop
    bool closed = false;
    u32 length = 0;            // millimeters (the loop for a closed path)
    bool valid() const { return points.size() >= 2 && length > 0; }
};
ScenePath readPath(const Entity& e);

// A joint object read for the simulation: names, kind and values with the anchor and axis in world space. Limits are radians for a hinge
// (the object's degrees, converted with one multiply), meters for a slider. The world axis is the object's rotation applied to +Y
struct SceneJoint {
    JointFields fields;
    Vec3 pivot;
    Vec3 axis{0.0f, 1.0f, 0.0f};
    f32 limitMin = 0.0f;
    f32 limitMax = 0.0f;
};
SceneJoint readJoint(const Entity& e);
// The point `millimeters` along the path: a closed path wraps around, an open one stops at its ends
Vec3 pathPoint(const ScenePath& path, u32 millimeters);
// Distance (millimeters) of the point on the path closest to p (the first one on a tie)
u32 pathClosest(const ScenePath& path, Vec3 p);

// Logs "<file>:<line>: object 'name' (id, kind): <warnings>" once per object and text (readObject may run every frame)
void warnObject(const Entity& e, const std::string& warnings);

// Reads the fields of a game kind. Fields not in the scene get the declaration's defaults.
template <class T>
T readObject(const Entity& e) {
    T value{};
    std::string warnings;
    reflect::fromText(reflect::typeOf<T>(), &value, e.fields, &warnings);
    if (!warnings.empty()) warnObject(e, warnings);
    return value;
}

// Field list filled with defaults (when the editor creates a new object)
reflect::TextFields defaultFields(const reflect::TypeInfo& type);

bool save(const Scene& scene, const std::string& path);

// Object kinds description for tools that edit scenes without the engine (MkRender): the built-in kinds and gameTypes with their
// flags and field lines (path, type, default text, label, enum names, flags). Format "object-kinds 1" (MealKit-Contract docs/CONTRACT.md
// "Object kinds"). Written to build/editor/kinds.txt by the editor when it loads the game code and by <game>_headless --write-kinds.
std::string objectKindsText(const TypeList* gameTypes);
bool writeObjectKinds(const std::string& path, const TypeList* gameTypes);
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

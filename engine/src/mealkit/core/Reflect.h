#pragma once

#include "mealkit/core/Hash.h"
#include "mealkit/core/Math.h"
#include "mealkit/core/TextFormat.h"

#include <array>
#include <cstring>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

// Field declarations (docs/CODING_MODEL.md "Data and settings"). Declare a struct's fields once and
// the editor inspector, text saving (.scene object fields, .tuning), state hash, snapshot save/restore, and live state view all use the same declaration.
//
//   MEALKIT_STRUCT(Tuning) {                       // struct with fields (tuning, simulation state)
//       MEALKIT_FIELD(f32, playerSpeed, 7.0f, "Move speed");
//       MEALKIT_ARRAY(Player, 8, players, "Players");  // std::array<Player, 8>
//   };
//   MEALKIT_OBJECT(JumpPad) {                      // scene object kind shown in the editor [Add] menu
//       static constexpr const char* kObjectLabel = "Jump pad";
//       static constexpr mealkit::u32 kObjectFlags = mealkit::reflect::kObjectArea; // (optional) TypeFlags
//       MEALKIT_FIELD(f32, power, 12.0f, "Jump power");
//   };
//
// Field types: bool, integers/enums, f32, Vec3, Quat, Pool Id, std::string, Ticks (time), Color, RowRef<Row> (table row), other MEALKIT_STRUCTs,
// MEALKIT_ARRAY, sim::Pool<T>.
// std::string is for settings/tuning/scene fields. Don't put it in simulation state (WorldState, Pool items) (game::Simulation checks).
// Wrap defaults containing commas in parentheses: MEALKIT_FIELD(Vec3, dir, (Vec3{0, 1, 0}), "Direction").
// Enums whose names appear in files/editor are declared with MEALKIT_ENUM(EventKind, u8, Push, Bounce, Spawn) (values in order from 0, no explicit values).
// Field order is the save/hash order. Don't put other macros that use __COUNTER__ between declarations.
namespace mealkit {

// Time values (docs/CODING_MODEL.md "Time values"): integer ticks in memory/hash/sim/, seconds in files and the editor.
// Rounded once with the process tick rate (reflect::tickRate(), set when a project opens) when reading files.
//   MEALKIT_FIELD(Ticks, respawn, Ticks::seconds(1.5f), "Respawn delay (s)");   // in sim/ world.tuning.respawn.value (also converts to u32)
struct Ticks {
    u32 value = 0;

    static Ticks seconds(f32 seconds); // rounded by the tick rate (negatives become 0)
    f32 toSeconds() const;
    constexpr operator u32() const { return value; }
};

// Row reference into a definition data table (docs/CODING_MODEL.md "Many items of the same shape"): the row name id in files/editor,
// and data::resolveRows fills in the row index after loading. sim/ reads by index (world.weapons[ref.index] or table.get(ref)).
// Contains a string, so not for simulation state (WorldState): keep a u32 row index in state.
inline constexpr u32 kNoRow = 0xFFFFFFFFu;

template <class Row>
struct RowRef {
    using RowType = Row;
    static constexpr bool kRowRef = true;
    std::string id;       // row name (empty = no reference)
    u32 index = kNoRow;   // set after loading (kNoRow for a missing id)

    RowRef() = default;
    RowRef(const char* rowId) : id(rowId) {}
    bool valid() const { return index != kNoRow; }
};

// Color (view value): rgba8 sRGB, same layout as render::rgba (r | g << 8 | b << 16 | a << 24). "#RRGGBB" or "#RRGGBBAA" in files.
struct Color {
    u32 value = 0xFFFFFFFFu;

    static constexpr Color rgb(u8 r, u8 g, u8 b, u8 a = 255) {
        return {static_cast<u32>(r) | (static_cast<u32>(g) << 8) | (static_cast<u32>(b) << 16) |
                (static_cast<u32>(a) << 24)};
    }
    constexpr bool operator==(const Color&) const = default;
};

} // namespace mealkit

namespace mealkit::reflect {

// Tick rate (Hz) used for Ticks text conversion. Set once right after reading project settings (game::loadGameProject, editor project open).
// A process setting like the project root (setProjectRoot): doesn't change during a match, and every peer uses the same value (content hash).
void setTickRate(u32 hz);
u32 tickRate();

// ---------------------------------------------------------------- Runtime description (so the editor and engine functions can handle types without knowing them)

enum class Kind : u8 {
    Bool,
    Int,   // signed integers/enums (size 1, 2, 4, 8)
    UInt,  // unsigned integers/enums
    Float, // f32
    Vec3,
    Quat,
    Id,        // sim::Id<T> (slot, generation)
    Struct,    // MEALKIT_STRUCT
    Array,     // MEALKIT_ARRAY
    Container, // sim::Pool<T>
    String,    // std::string (in text files, to end of line, no leading/trailing spaces)
    Ticks,     // mealkit::Ticks (u32 ticks in memory, seconds in text)
    Color,     // mealkit::Color (u32 rgba8 in memory, #RRGGBB[AA] in text)
    RowRef,    // mealkit::RowRef<Row> (type = row type, row id in text)
};

enum FieldFlags : u32 {
    kFieldView = 1u << 0,     // display-only value: excluded from the content hash (MEALKIT_VIEW_FIELD)
    kFieldReadOnly = 1u << 1, // value not edited in the editor (written by tools): shows the value instead of a widget
};

enum TypeFlags : u32 {
    kObjectCosmetic = 1u << 0, // object with no effect on game results (decoration): excluded from the content hash
    kObjectMarker = 1u << 1,   // marker that only uses position (spawn point): the editor draws it at a fixed size, the game doesn't draw it
    kObjectArea = 1u << 2,     // invisible area that only uses size (spawn area, match bounds): the editor draws only corners, the game doesn't draw it
};

class ByteWriter {
public:
    void bytes(const void* data, size_t size) {
        const u8* p = static_cast<const u8*>(data);
        m_data.insert(m_data.end(), p, p + size);
    }
    template <class T>
    void value(const T& v) {
        static_assert(std::is_trivially_copyable_v<T>);
        bytes(&v, sizeof(T));
    }
    std::vector<u8>& data() { return m_data; }

private:
    std::vector<u8> m_data;
};

class ByteReader {
public:
    ByteReader(const u8* data, size_t size) : m_data(data), m_size(size) {}
    bool bytes(void* out, size_t size) {
        if (m_pos + size > m_size) return false;
        std::memcpy(out, m_data + m_pos, size);
        m_pos += size;
        return true;
    }
    template <class T>
    bool value(T& v) {
        static_assert(std::is_trivially_copyable_v<T>);
        return bytes(&v, sizeof(T));
    }
    bool atEnd() const { return m_pos == m_size; }

private:
    const u8* m_data;
    size_t m_size;
    size_t m_pos = 0;
};

struct TypeInfo;
struct ValueInfo;

// Containers with slots, like sim::Pool
struct ContainerOps {
    u32 (*capacity)(const void* container);
    bool (*live)(const void* container, u32 slot);
    const void* (*item)(const void* container, u32 slot);
    u32 (*generation)(const void* container, u32 slot);
    void (*hashSlots)(const void* container, Hasher& h); // slot state (excluding items)
    void (*save)(const void* container, ByteWriter& w);
    bool (*load)(void* container, ByteReader& r);
};

struct ValueInfo {
    Kind kind = Kind::Bool;
    u32 size = 0;
    std::vector<std::string> enumNames; // MEALKIT_ENUM: names of values 0, 1, 2 ... (names in files/editor, integers in hash/memory)
    const TypeInfo* type = nullptr;     // Struct, RowRef (row type of the referenced table)
    const ValueInfo* element = nullptr; // Array, Container
    u32 count = 0;                      // Array
    const ContainerOps* ops = nullptr;  // Container
};

struct FieldInfo {
    const char* name;  // file key (English, the field name in code)
    const char* label; // editor display name
    u32 offset;
    u32 flags; // FieldFlags
    const ValueInfo* value;
};

struct TypeInfo {
    const char* name;  // type name in code (kind value in scene files)
    const char* label; // editor display name (kObjectLabel, or name if absent)
    u32 size = 0;
    u32 flags = 0; // TypeFlags (kObjectFlags)
    std::vector<FieldInfo> fields;
    void* (*create)() = nullptr; // instance created with defaults (deleted with destroy). Uses the heap of the module that defined the type
    void (*destroy)(void*) = nullptr;
    void (*copy)(void* dst, const void* src) = nullptr;

    const FieldInfo* field(const char* fieldName) const;
};

// For showing a single value to code that doesn't know the type (the editor)
struct View {
    const TypeInfo* type = nullptr;
    const void* data = nullptr;
};

// Instance created and destroyed by functions of the module that defined the type (when the editor edits types from the game DLL)
class Instance {
public:
    Instance() = default;
    explicit Instance(const TypeInfo& type) : m_type(&type), m_data(type.create()) {}
    Instance(const Instance&) = delete;
    Instance& operator=(const Instance&) = delete;
    Instance(Instance&& o) noexcept : m_type(o.m_type), m_data(o.m_data) { o.m_data = nullptr; }
    Instance& operator=(Instance&& o) noexcept {
        std::swap(m_type, o.m_type);
        std::swap(m_data, o.m_data);
        return *this;
    }
    ~Instance() {
        if (m_data) m_type->destroy(m_data);
    }
    const TypeInfo* type() const { return m_type; }
    void* data() { return m_data; }
    const void* data() const { return m_data; }

private:
    const TypeInfo* m_type = nullptr;
    void* m_data = nullptr;
};

// ---------------------------------------------------------------- Engine functions using the runtime description (Reflect.cpp)

// State hash: fields are added one by one (excluding padding). Fields matching skipFlags are skipped (kFieldView for the content hash).
void hashValue(const ValueInfo& value, const void* data, Hasher& h, u32 skipFlags = 0);
void hashStruct(const TypeInfo& type, const void* data, Hasher& h, u32 skipFlags = 0);

// Snapshots: save and restore within the same process and build (rewind, shadow simulation).
void saveStruct(const TypeInfo& type, const void* data, ByteWriter& w);
bool loadStruct(const TypeInfo& type, void* data, ByteReader& r);

// Text: "path value" lines. Paths are field names, a.b for nesting, a[2] for arrays. Containers (Pool) aren't written as text.
using TextFields = std::vector<std::pair<std::string, std::string>>;
void toText(const TypeInfo& type, const void* data, TextFields& out);
// Reads one path into the value. false for a missing path or bad value (value unchanged).
bool setFromText(const TypeInfo& type, void* data, const std::string& path, const std::string& text);
// Reads multiple lines, appending failed paths to warnings.
void fromText(const TypeInfo& type, void* data, const TextFields& fields, std::string* warnings = nullptr);
// One value at path "a.b[2].c" (into structs/arrays). false if missing.
bool resolvePath(const TypeInfo& type, void* data, const std::string& path, const ValueInfo*& outValue, void*& outData);
// id and row index of a RowRef value (data of Kind::RowRef)
struct RowRefView {
    std::string* id;
    u32* index;
};
RowRefView rowRefOf(void* data);
std::string formatScalar(const ValueInfo& value, const void* data); // for debug display (empty string for structs/arrays)

// Text files like .tuning ("# comment", format line "@format <kind> <N>", "path value")
bool saveTextFile(const TypeInfo& type, const void* data, const std::string& path, const text::Format& format, const char* header);
bool loadTextFile(const TypeInfo& type, void* data, const std::string& path, const text::Format& format, std::string* error = nullptr);

// Fields that differ between two values as "path: a -> b" lines (shadow simulation desync reports). At most maxLines lines.
void diffStruct(const TypeInfo& type, const void* a, const void* b, std::vector<std::string>& out, u32 maxLines = 32);

// ---------------------------------------------------------------- Compile-time declarations

template <class T>
struct Reflected {
    using MealkitSelf = T;
};

template <int N>
struct FieldSlot {};

template <class C, class V>
struct FieldDecl {
    using Value = V;
    V C::* member;
    const char* name;
    const char* label;
    u32 flags;
};

template <class C, class V>
constexpr FieldDecl<C, V> makeDecl(V C::* member, const char* name, const char* label, u32 flags) {
    return {member, name, label, flags};
}

template <class T>
inline constexpr bool isReflected = std::is_base_of_v<Reflected<T>, T>;

template <class T>
struct IsStdArray : std::false_type {};
template <class E, size_t N>
struct IsStdArray<std::array<E, N>> : std::true_type {};

// Calls f(FieldDecl) in declaration order
template <class T, int I = 0, class F>
constexpr void forEachField(F&& f) {
    if constexpr (requires { T::mealkitField(FieldSlot<I>{}); }) {
        f(T::mealkitField(FieldSlot<I>{}));
        forEachField<T, I + 1>(f);
    }
}

template <class T>
const TypeInfo& typeOf();

template <class V>
const ValueInfo& valueOf();

// "Push, Bounce, Spawn" → {"Push", "Bounce", "Spawn"} (MEALKIT_ENUM)
std::vector<std::string> splitEnumNames(const char* text);

template <class>
inline constexpr bool kAlwaysFalse = false;


template <class V>
ValueInfo makeValue() {
    ValueInfo v;
    v.size = static_cast<u32>(sizeof(V));
    if constexpr (std::is_same_v<V, bool>) {
        v.kind = Kind::Bool;
    } else if constexpr (std::is_enum_v<V>) {
        v.kind = std::is_signed_v<std::underlying_type_t<V>> ? Kind::Int : Kind::UInt;
        if constexpr (requires { mealkitEnumText(static_cast<V*>(nullptr)); })
            v.enumNames = splitEnumNames(mealkitEnumText(static_cast<V*>(nullptr)));
    } else if constexpr (std::is_integral_v<V>) {
        v.kind = std::is_signed_v<V> ? Kind::Int : Kind::UInt;
    } else if constexpr (std::is_same_v<V, f32>) {
        v.kind = Kind::Float;
    } else if constexpr (std::is_same_v<V, Vec3>) {
        v.kind = Kind::Vec3;
    } else if constexpr (std::is_same_v<V, Quat>) {
        v.kind = Kind::Quat;
    } else if constexpr (std::is_same_v<V, std::string>) {
        v.kind = Kind::String;
    } else if constexpr (std::is_same_v<V, Ticks>) {
        v.kind = Kind::Ticks;
    } else if constexpr (std::is_same_v<V, Color>) {
        v.kind = Kind::Color;
    } else if constexpr (requires { V::kRowRef; }) {
        v.kind = Kind::RowRef;
        v.type = &typeOf<typename V::RowType>();
    } else if constexpr (requires { V::kReflectId; }) {
        v.kind = Kind::Id;
    } else if constexpr (IsStdArray<V>::value) {
        v.kind = Kind::Array;
        v.element = &valueOf<typename V::value_type>();
        v.count = static_cast<u32>(std::tuple_size_v<V>);
    } else if constexpr (requires { V::reflectOps(); }) {
        v.kind = Kind::Container;
        v.element = &valueOf<typename V::value_type>();
        v.ops = &V::reflectOps();
    } else if constexpr (isReflected<V>) {
        v.kind = Kind::Struct;
        v.type = &typeOf<V>();
    } else {
        static_assert(kAlwaysFalse<V>, "MEALKIT_FIELD type is not supported (see mealkit/core/Reflect.h)");
    }
    return v;
}

template <class V>
const ValueInfo& valueOf() {
    static const ValueInfo info = makeValue<V>();
    return info;
}

template <class T>
TypeInfo makeType() {
    static_assert(isReflected<T>, "declare the type with MEALKIT_STRUCT or MEALKIT_OBJECT");
    TypeInfo t;
    t.name = mealkitTypeName(static_cast<const T*>(nullptr));
    t.label = t.name;
    if constexpr (requires { T::kObjectLabel; }) t.label = T::kObjectLabel;
    if constexpr (requires { T::kObjectFlags; }) t.flags = T::kObjectFlags;
    t.size = static_cast<u32>(sizeof(T));
    const std::unique_ptr<T> probe = std::make_unique<T>(); // for computing field offsets
    const char* base = reinterpret_cast<const char*>(probe.get());
    forEachField<T>([&](auto decl) {
        using V = std::remove_cvref_t<decltype(probe.get()->*decl.member)>;
        const char* at = reinterpret_cast<const char*>(&(probe.get()->*decl.member));
        t.fields.push_back({decl.name, decl.label && decl.label[0] ? decl.label : decl.name,
                            static_cast<u32>(at - base), decl.flags, &valueOf<V>()});
    });
    t.create = []() -> void* { return new T(); };
    t.destroy = [](void* p) { delete static_cast<T*>(p); };
    t.copy = [](void* dst, const void* src) { *static_cast<T*>(dst) = *static_cast<const T*>(src); };
    return t;
}

template <class T>
const TypeInfo& typeOf() {
    static const TypeInfo info = makeType<T>();
    return info;
}

// Name of a MEALKIT_ENUM value (empty string for values without a name)
template <class E>
const std::string& enumName(E value) {
    static const std::string none;
    const std::vector<std::string>& names = valueOf<E>().enumNames;
    const auto i = static_cast<size_t>(value);
    return i < names.size() ? names[i] : none;
}

// Whether V (or structs/arrays inside it) has std::string fields. Simulation state is copied/hashed byte-wise, so no strings.
template <class V>
constexpr bool hasStringField() {
    if constexpr (std::is_same_v<V, std::string> || requires { V::kRowRef; }) {
        return true;
    } else if constexpr (IsStdArray<V>::value) {
        return hasStringField<typename V::value_type>();
    } else if constexpr (isReflected<V>) {
        bool found = false;
        forEachField<V>([&](auto decl) { found = found || hasStringField<typename decltype(decl)::Value>(); });
        return found;
    } else {
        return false; // Pool checks that its items are trivially copyable
    }
}

// Convenience functions for code that knows the type (game code)
template <class T>
u64 hashOf(const T& value, u32 skipFlags = 0) {
    Hasher h;
    hashStruct(typeOf<T>(), &value, h, skipFlags);
    return h.value;
}

// Definition data table registration (MEALKIT_TABLE). Table file is a path relative to assets, row type is a MEALKIT_STRUCT.
enum TableFlags : u32 {
    kTableView = 1u << 0, // view-only table (sounds/feedback/icons): outside the content hash, reloaded immediately when edited during play
};

struct TableInfo {
    const TypeInfo* rowType;
    const char* path; // relative to assets ("tables/props.table")
    u32 flags;        // TableFlags
};
using TableList = std::vector<TableInfo>;

// MEALKIT_TABLE list declared in this module (game exe or game DLL). The engine (mealkit.dll) doesn't call it (the game DLL passes it via MEALKIT_GAME_MODULE).
inline TableList& moduleTables() {
    static TableList list;
    return list;
}

template <class Row>
bool registerTable(const char* path, u32 flags) {
    moduleTables().push_back({&typeOf<Row>(), path, flags});
    return true;
}

// MEALKIT_OBJECT list declared in this module (game exe or game DLL). The engine (mealkit.dll) doesn't call this function.
// The game DLL passes it to the editor via MEALKIT_GAME_MODULE.
inline std::vector<const TypeInfo*>& moduleObjectTypes() {
    static std::vector<const TypeInfo*> list;
    return list;
}

template <class T>
bool registerObjectType() {
    moduleObjectTypes().push_back(&typeOf<T>());
    return true;
}

// MEALKIT_VIEWMODEL list declared in this module (game screen UI bindings, editor [UI] preview). The game DLL passes it via MEALKIT_GAME_MODULE.
inline std::vector<const TypeInfo*>& moduleViewModels() {
    static std::vector<const TypeInfo*> list;
    return list;
}

template <class T>
bool registerViewModel() {
    moduleViewModels().push_back(&typeOf<T>());
    return true;
}

} // namespace mealkit::reflect

// ---------------------------------------------------------------- Macros

#define MEALKIT_DETAIL_STRUCT_HEAD(Name)                                                                               \
    struct Name;                                                                                                       \
    [[maybe_unused]] static constexpr int mealkitFieldBase(const Name*) { return __COUNTER__; }                        \
    constexpr const char* mealkitTypeName(const Name*) { return #Name; }

#define MEALKIT_STRUCT(Name)                                                                                           \
    MEALKIT_DETAIL_STRUCT_HEAD(Name)                                                                                   \
    struct Name : ::mealkit::reflect::Reflected<Name>

#define MEALKIT_OBJECT(Name)                                                                                           \
    MEALKIT_DETAIL_STRUCT_HEAD(Name)                                                                                   \
    inline const bool mealkitObjectRegistered_##Name = ::mealkit::reflect::registerObjectType<Name>();                 \
    struct Name : ::mealkit::reflect::Reflected<Name>

// View model for game screen UI (mealkit/ui/Ui.h): the view side fills state → view model every frame, and .ui binds by field name.
#define MEALKIT_VIEWMODEL(Name)                                                                                        \
    MEALKIT_DETAIL_STRUCT_HEAD(Name)                                                                                   \
    inline const bool mealkitViewModelRegistered_##Name = ::mealkit::reflect::registerViewModel<Name>();               \
    struct Name : ::mealkit::reflect::Reflected<Name>

#define MEALKIT_DETAIL_DECL(name, label, flags)                                                                        \
    static constexpr auto mealkitField(                                                                                \
        ::mealkit::reflect::FieldSlot<__COUNTER__ - mealkitFieldBase(static_cast<const MealkitSelf*>(nullptr)) - 1>) { \
        return ::mealkit::reflect::makeDecl(&MealkitSelf::name, #name, label, flags);                                  \
    }

#define MEALKIT_FIELD(Type, name, def, label)                                                                          \
    Type name = def;                                                                                                   \
    MEALKIT_DETAIL_DECL(name, label, 0u)

// Field excluded from the content hash (display-only values like colors and effects)
#define MEALKIT_VIEW_FIELD(Type, name, def, label)                                                                     \
    Type name = def;                                                                                                   \
    MEALKIT_DETAIL_DECL(name, label, ::mealkit::reflect::kFieldView)

// Field with explicit flags (reflect::kFieldView | reflect::kFieldReadOnly etc.)
#define MEALKIT_FIELD_FLAGS(Type, name, def, label, flags)                                                             \
    Type name = def;                                                                                                   \
    MEALKIT_DETAIL_DECL(name, label, static_cast<::mealkit::u32>(flags))

// Definition data table: MEALKIT_TABLE(PropKind, "tables/props.table", 0) — row type is a MEALKIT_STRUCT, flags is reflect::TableFlags
#define MEALKIT_TABLE(Row, path, flags)                                                                                \
    inline const bool mealkitTableRegistered_##Row = ::mealkit::reflect::registerTable<Row>(path, flags)

// Named enum: MEALKIT_ENUM(EventKind, u8, Push, Bounce) — values in order from 0. Shown by name in files and the editor.
#define MEALKIT_ENUM(Name, Underlying, ...)                                                                            \
    enum class Name : Underlying { __VA_ARGS__ };                                                                      \
    [[maybe_unused]] constexpr const char* mealkitEnumText(Name*) { return #__VA_ARGS__; }

#define MEALKIT_ARRAY(Type, count, name, label)                                                                        \
    ::std::array<Type, count> name{};                                                                                  \
    MEALKIT_DETAIL_DECL(name, label, 0u)

// View model list: array name of at most count items and the used count nameCount (.ui "list name" repeats the template that many times)
#define MEALKIT_LIST(Type, name, count, label)                                                                         \
    MEALKIT_FIELD(::mealkit::u32, name##Count, 0, label " (count)")                                                    \
    MEALKIT_ARRAY(Type, count, name, label)

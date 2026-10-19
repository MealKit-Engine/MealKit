#pragma once

#include "mealkit/core/Hash.h"
#include "mealkit/core/Math.h"

#include <array>
#include <cstring>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

// 필드 선언 (docs/CODING_MODEL.md "데이터와 설정"). 구조체의 필드를 한 번 선언하면
// 에디터 인스펙터, 텍스트 저장(.scene 오브젝트 필드, .tuning), 상태 해시, 스냅샷 저장·복원, 실시간 상태 보기가 모두 같은 선언을 쓴다.
//
//   MEALKIT_STRUCT(Tuning) {                       // 필드가 있는 구조체 (튜닝, 시뮬레이션 상태)
//       MEALKIT_FIELD(f32, playerSpeed, 7.0f, "이동 속도");
//       MEALKIT_ARRAY(Player, 8, players, "플레이어");  // std::array<Player, 8>
//   };
//   MEALKIT_OBJECT(JumpPad) {                      // 에디터 [추가] 메뉴에 나오는 씬 오브젝트 종류
//       static constexpr const char* kObjectLabel = "점프대";
//       static constexpr mealkit::u32 kObjectFlags = mealkit::reflect::kObjectArea; // (선택) TypeFlags
//       MEALKIT_FIELD(f32, power, 12.0f, "점프 힘");
//   };
//
// 필드 타입: bool, 정수·enum, f32, Vec3, Quat, Pool의 Id, std::string, Ticks(시간), Color(색), RowRef<Row>(표의 행), 다른 MEALKIT_STRUCT,
// MEALKIT_ARRAY, sim::Pool<T>.
// std::string은 설정·튜닝·씬 필드용이다. 시뮬레이션 상태(WorldState, Pool 항목)에는 넣지 않는다(game::Simulation이 검사한다).
// 기본값에 쉼표가 들어가면 괄호로 감싼다: MEALKIT_FIELD(Vec3, dir, (Vec3{0, 1, 0}), "방향").
// 이름이 파일·에디터에 보이는 enum은 MEALKIT_ENUM(EventKind, u8, Push, Bounce, Spawn)으로 선언한다 (값은 0부터 차례로, 직접 값을 주지 않는다).
// 필드 순서가 곧 저장·해시 순서다. 선언 사이에 __COUNTER__를 쓰는 다른 매크로를 넣지 않는다.
namespace mealkit {

// 시간 값 (docs/CODING_MODEL.md "시간 값"): 메모리·해시·sim/에서는 정수 틱, 파일과 에디터에서는 초.
// 파일을 읽을 때 프로세스 틱레이트(reflect::tickRate(), 프로젝트를 열 때 정한다)로 한 번 반올림한다.
//   MEALKIT_FIELD(Ticks, respawn, Ticks::seconds(1.5f), "리스폰 대기 (초)");   // sim/에서는 world.tuning.respawn.value (u32로도 바뀐다)
struct Ticks {
    u32 value = 0;

    static Ticks seconds(f32 seconds); // 틱레이트로 반올림 (음수는 0)
    f32 toSeconds() const;
    constexpr operator u32() const { return value; }
};

// 정의 데이터 표의 행 참조 (docs/CODING_MODEL.md "같은 모양의 데이터가 여러 개"): 파일·에디터에는 행 이름 id,
// 불러온 뒤 data::resolveRows가 행 번호(index)를 채운다. sim/은 번호로 읽는다(world.weapons[ref.index] 또는 table.get(ref)).
// 문자열이 들어 있어 시뮬레이션 상태(WorldState)에는 넣지 않는다: 상태에는 u32 행 번호를 둔다.
inline constexpr u32 kNoRow = 0xFFFFFFFFu;

template <class Row>
struct RowRef {
    using RowType = Row;
    static constexpr bool kRowRef = true;
    std::string id;       // 행 이름 (비어 있으면 참조 없음)
    u32 index = kNoRow;   // 불러온 뒤 정해진다 (없는 id면 kNoRow)

    RowRef() = default;
    RowRef(const char* rowId) : id(rowId) {}
    bool valid() const { return index != kNoRow; }
};

// 색 (화면용 값): rgba8 sRGB, render::rgba와 같은 배치(r | g << 8 | b << 16 | a << 24). 파일에는 "#RRGGBB" 또는 "#RRGGBBAA".
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

// Ticks 텍스트 변환에 쓰는 틱레이트 (Hz). 프로젝트 설정을 읽은 직후 한 번 정한다 (game::loadGameProject, 에디터의 프로젝트 열기).
// 프로젝트 루트(setProjectRoot)처럼 프로세스 설정이다: 매치 중에는 바뀌지 않고, 모든 피어가 같은 값을 쓴다(콘텐츠 해시).
void setTickRate(u32 hz);
u32 tickRate();

// ---------------------------------------------------------------- 실행 중 설명 (에디터와 엔진 함수가 타입을 몰라도 다룰 수 있게)

enum class Kind : u8 {
    Bool,
    Int,   // 부호 있는 정수·enum (size 1, 2, 4, 8)
    UInt,  // 부호 없는 정수·enum
    Float, // f32
    Vec3,
    Quat,
    Id,        // sim::Id<T> (slot, generation)
    Struct,    // MEALKIT_STRUCT
    Array,     // MEALKIT_ARRAY
    Container, // sim::Pool<T>
    String,    // std::string (텍스트 파일에서는 줄 끝까지, 앞뒤 공백 없음)
    Ticks,     // mealkit::Ticks (메모리는 u32 틱, 텍스트는 초)
    Color,     // mealkit::Color (메모리는 u32 rgba8, 텍스트는 #RRGGBB[AA])
    RowRef,    // mealkit::RowRef<Row> (type = 행 타입, 텍스트는 행 id)
};

enum FieldFlags : u32 {
    kFieldView = 1u << 0,     // 보이기만 하는 값: 콘텐츠 해시에서 뺀다 (MEALKIT_VIEW_FIELD)
    kFieldReadOnly = 1u << 1, // 에디터에서 고치지 않는 값 (도구가 쓴다): 위젯 대신 값만 보여 준다
};

enum TypeFlags : u32 {
    kObjectCosmetic = 1u << 0, // 게임 결과에 영향이 없는 오브젝트 (장식): 콘텐츠 해시에서 뺀다
    kObjectMarker = 1u << 1,   // 위치만 쓰는 표시 (스폰 지점): 에디터는 고정 크기로 그리고, 게임은 그리지 않는다
    kObjectArea = 1u << 2,     // 크기만 쓰는 보이지 않는 영역 (생성 영역, 경기 범위): 에디터는 모서리만 그리고, 게임은 그리지 않는다
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

// sim::Pool처럼 슬롯이 있는 컨테이너
struct ContainerOps {
    u32 (*capacity)(const void* container);
    bool (*live)(const void* container, u32 slot);
    const void* (*item)(const void* container, u32 slot);
    u32 (*generation)(const void* container, u32 slot);
    void (*hashSlots)(const void* container, Hasher& h); // 슬롯 상태(항목 제외)
    void (*save)(const void* container, ByteWriter& w);
    bool (*load)(void* container, ByteReader& r);
};

struct ValueInfo {
    Kind kind = Kind::Bool;
    u32 size = 0;
    std::vector<std::string> enumNames; // MEALKIT_ENUM: 값 0, 1, 2 ...의 이름 (파일·에디터는 이름, 해시·메모리는 정수)
    const TypeInfo* type = nullptr;     // Struct, RowRef(가리키는 표의 행 타입)
    const ValueInfo* element = nullptr; // Array, Container
    u32 count = 0;                      // Array
    const ContainerOps* ops = nullptr;  // Container
};

struct FieldInfo {
    const char* name;  // 파일 키 (영어, 코드의 필드 이름)
    const char* label; // 에디터 표시 이름
    u32 offset;
    u32 flags; // FieldFlags
    const ValueInfo* value;
};

struct TypeInfo {
    const char* name;  // 코드의 타입 이름 (씬 파일의 kind 값)
    const char* label; // 에디터 표시 이름 (kObjectLabel, 없으면 name)
    u32 size = 0;
    u32 flags = 0; // TypeFlags (kObjectFlags)
    std::vector<FieldInfo> fields;
    void* (*create)() = nullptr; // 기본값으로 만든 인스턴스 (destroy로 지운다). 타입을 정의한 모듈의 힙을 쓴다
    void (*destroy)(void*) = nullptr;
    void (*copy)(void* dst, const void* src) = nullptr;

    const FieldInfo* field(const char* fieldName) const;
};

// 타입을 모르는 쪽(에디터)에 값 하나를 보여 줄 때
struct View {
    const TypeInfo* type = nullptr;
    const void* data = nullptr;
};

// 타입을 정의한 모듈 쪽 함수로 만들고 지우는 인스턴스 (에디터가 게임 DLL의 타입을 편집할 때)
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

// ---------------------------------------------------------------- 실행 중 설명을 쓰는 엔진 함수 (Reflect.cpp)

// 상태 해시: 필드를 하나씩 넣는다(패딩 제외). skipFlags에 걸린 필드는 뺀다(콘텐츠 해시에서 kFieldView).
void hashValue(const ValueInfo& value, const void* data, Hasher& h, u32 skipFlags = 0);
void hashStruct(const TypeInfo& type, const void* data, Hasher& h, u32 skipFlags = 0);

// 스냅샷: 같은 프로세스·같은 빌드 안에서 저장하고 되돌린다 (되감기, 그림자 시뮬레이션).
void saveStruct(const TypeInfo& type, const void* data, ByteWriter& w);
bool loadStruct(const TypeInfo& type, void* data, ByteReader& r);

// 텍스트: "경로 값" 줄들. 경로는 필드 이름, 중첩은 a.b, 배열은 a[2]. 컨테이너(Pool)는 텍스트로 쓰지 않는다.
using TextFields = std::vector<std::pair<std::string, std::string>>;
void toText(const TypeInfo& type, const void* data, TextFields& out);
// 경로 하나를 읽어 넣는다. 없는 경로나 잘못된 값이면 false (값은 그대로).
bool setFromText(const TypeInfo& type, void* data, const std::string& path, const std::string& text);
// 여러 줄을 읽어 넣고, 실패한 경로는 warnings에 붙인다.
void fromText(const TypeInfo& type, void* data, const TextFields& fields, std::string* warnings = nullptr);
// "a.b[2].c" 경로의 값 하나 (구조체·배열 안까지). 없으면 false.
bool resolvePath(const TypeInfo& type, void* data, const std::string& path, const ValueInfo*& outValue, void*& outData);
// RowRef 값 (Kind::RowRef인 data)의 id와 행 번호
struct RowRefView {
    std::string* id;
    u32* index;
};
RowRefView rowRefOf(void* data);
std::string formatScalar(const ValueInfo& value, const void* data); // 디버그 표시용 (구조체·배열은 빈 문자열)

// .tuning 같은 텍스트 파일 ("# 주석", "경로 값")
bool saveTextFile(const TypeInfo& type, const void* data, const std::string& path, const char* header);
bool loadTextFile(const TypeInfo& type, void* data, const std::string& path, std::string* error = nullptr);

// 두 값의 다른 필드를 "경로: a -> b" 줄로 (그림자 시뮬레이션 디싱크 보고). 최대 maxLines 줄.
void diffStruct(const TypeInfo& type, const void* a, const void* b, std::vector<std::string>& out, u32 maxLines = 32);

// ---------------------------------------------------------------- 컴파일 시간 선언

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

// 선언 순서대로 f(FieldDecl)를 부른다
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
    const std::unique_ptr<T> probe = std::make_unique<T>(); // 필드 위치 계산용
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

// MEALKIT_ENUM 값의 이름 (이름이 없는 값이면 빈 문자열)
template <class E>
const std::string& enumName(E value) {
    static const std::string none;
    const std::vector<std::string>& names = valueOf<E>().enumNames;
    const auto i = static_cast<size_t>(value);
    return i < names.size() ? names[i] : none;
}

// V(또는 그 안의 구조체·배열)에 std::string 필드가 있는가. 시뮬레이션 상태는 복사·해시가 바이트 단위라 문자열을 넣지 않는다.
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
        return false; // Pool 항목은 Pool이 trivially copyable을 검사한다
    }
}

// 타입을 아는 쪽(게임 코드)에서 쓰는 편의 함수
template <class T>
u64 hashOf(const T& value, u32 skipFlags = 0) {
    Hasher h;
    hashStruct(typeOf<T>(), &value, h, skipFlags);
    return h.value;
}

// 정의 데이터 표 등록 (MEALKIT_TABLE). 표 파일은 assets 기준 경로, 행 타입은 MEALKIT_STRUCT.
enum TableFlags : u32 {
    kTableView = 1u << 0, // 화면 전용 표 (소리·연출·아이콘): 콘텐츠 해시 밖, 플레이 중에 고치면 바로 다시 읽는다
};

struct TableInfo {
    const TypeInfo* rowType;
    const char* path; // assets 기준 ("tables/props.table")
    u32 flags;        // TableFlags
};
using TableList = std::vector<TableInfo>;

// 이 모듈(게임 exe 또는 게임 DLL)에 선언된 MEALKIT_TABLE 목록. 엔진(mealkit.dll)은 부르지 않는다(게임 DLL은 MEALKIT_GAME_MODULE로 넘긴다).
inline TableList& moduleTables() {
    static TableList list;
    return list;
}

template <class Row>
bool registerTable(const char* path, u32 flags) {
    moduleTables().push_back({&typeOf<Row>(), path, flags});
    return true;
}

// 이 모듈(게임 exe 또는 게임 DLL)에 선언된 MEALKIT_OBJECT 목록. 엔진(mealkit.dll)은 이 함수를 부르지 않는다.
// 게임 DLL은 MEALKIT_GAME_MODULE로 에디터에 넘긴다.
inline std::vector<const TypeInfo*>& moduleObjectTypes() {
    static std::vector<const TypeInfo*> list;
    return list;
}

template <class T>
bool registerObjectType() {
    moduleObjectTypes().push_back(&typeOf<T>());
    return true;
}

// 이 모듈에 선언된 MEALKIT_VIEWMODEL 목록 (게임 화면 UI의 바인딩, 에디터 [UI] 미리보기). 게임 DLL은 MEALKIT_GAME_MODULE로 넘긴다.
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

// ---------------------------------------------------------------- 매크로

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

// 게임 화면 UI의 뷰 모델 (mealkit/ui/Ui.h): 화면 쪽이 매 프레임 상태 → 뷰 모델을 채우고, .ui가 필드 이름으로 바인딩한다.
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

// 콘텐츠 해시에서 빠지는 필드 (색, 효과 같은 보이기만 하는 값)
#define MEALKIT_VIEW_FIELD(Type, name, def, label)                                                                     \
    Type name = def;                                                                                                   \
    MEALKIT_DETAIL_DECL(name, label, ::mealkit::reflect::kFieldView)

// 플래그를 직접 주는 필드 (reflect::kFieldView | reflect::kFieldReadOnly 등)
#define MEALKIT_FIELD_FLAGS(Type, name, def, label, flags)                                                             \
    Type name = def;                                                                                                   \
    MEALKIT_DETAIL_DECL(name, label, static_cast<::mealkit::u32>(flags))

// 정의 데이터 표: MEALKIT_TABLE(PropKind, "tables/props.table", 0) — 행 타입은 MEALKIT_STRUCT, flags는 reflect::TableFlags
#define MEALKIT_TABLE(Row, path, flags)                                                                                \
    inline const bool mealkitTableRegistered_##Row = ::mealkit::reflect::registerTable<Row>(path, flags)

// 이름이 있는 enum: MEALKIT_ENUM(EventKind, u8, Push, Bounce) — 값은 0부터 차례로. 파일과 에디터에는 이름으로 보인다.
#define MEALKIT_ENUM(Name, Underlying, ...)                                                                            \
    enum class Name : Underlying { __VA_ARGS__ };                                                                      \
    [[maybe_unused]] constexpr const char* mealkitEnumText(Name*) { return #__VA_ARGS__; }

#define MEALKIT_ARRAY(Type, count, name, label)                                                                        \
    ::std::array<Type, count> name{};                                                                                  \
    MEALKIT_DETAIL_DECL(name, label, 0u)

// 뷰 모델의 목록: 최대 count개 배열 name과 쓰는 개수 nameCount (.ui의 "list name"이 개수만큼 템플릿을 반복한다)
#define MEALKIT_LIST(Type, name, count, label)                                                                         \
    MEALKIT_FIELD(::mealkit::u32, name##Count, 0, label " (count)")                                                    \
    MEALKIT_ARRAY(Type, count, name, label)

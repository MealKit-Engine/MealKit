#include "mealkit/core/Reflect.h"

#include "mealkit/core/Log.h"
#include "mealkit/core/TextFormat.h"
#include "mealkit/platform/Paths.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace mealkit {
namespace {

u32 g_tickRate = 30; // reflect::setTickRate (프로세스 설정, 매치 중에는 바뀌지 않는다)

} // namespace

Ticks Ticks::seconds(f32 seconds) {
    if (!(seconds > 0.0f)) return {};
    return {static_cast<u32>(std::lround(static_cast<f64>(seconds) * static_cast<f64>(g_tickRate)))};
}

f32 Ticks::toSeconds() const { return static_cast<f32>(static_cast<f64>(value) / static_cast<f64>(g_tickRate)); }

} // namespace mealkit

namespace mealkit::reflect {

void setTickRate(u32 hz) { g_tickRate = hz ? hz : 30u; }
u32 tickRate() { return g_tickRate; }

namespace {

const u8* at(const void* data, u32 offset) { return static_cast<const u8*>(data) + offset; }
u8* at(void* data, u32 offset) { return static_cast<u8*>(data) + offset; }

i64 readInt(const void* p, u32 size) {
    switch (size) {
    case 1: return *static_cast<const i8*>(p);
    case 2: return *static_cast<const i16*>(p);
    case 4: return *static_cast<const i32*>(p);
    default: return *static_cast<const i64*>(p);
    }
}

u64 readUInt(const void* p, u32 size) {
    switch (size) {
    case 1: return *static_cast<const u8*>(p);
    case 2: return *static_cast<const u16*>(p);
    case 4: return *static_cast<const u32*>(p);
    default: return *static_cast<const u64*>(p);
    }
}

template <class T>
void store(void* p, T v) {
    std::memcpy(p, &v, sizeof(T));
}

bool parseFloats(const char* text, f32* out, int count) {
    const char* p = text;
    for (int i = 0; i < count; ++i) {
        char* end = nullptr;
        out[i] = std::strtof(p, &end);
        if (end == p) return false;
        p = end;
    }
    while (*p == ' ' || *p == '\t') ++p;
    return *p == '\0';
}

bool parseScalar(const ValueInfo& v, void* data, const std::string& text) {
    const char* s = text.c_str();
    char* end = nullptr;
    switch (v.kind) {
    case Kind::Bool:
        if (text == "true" || text == "1") *static_cast<bool*>(data) = true;
        else if (text == "false" || text == "0") *static_cast<bool*>(data) = false;
        else return false;
        return true;
    case Kind::Int: {
        long long x = std::strtoll(s, &end, 10);
        if (end == s || *end) {
            const auto it = std::find(v.enumNames.begin(), v.enumNames.end(), text); // MEALKIT_ENUM 이름
            if (it == v.enumNames.end()) return false;
            x = it - v.enumNames.begin();
        }
        switch (v.size) {
        case 1: store(data, static_cast<i8>(x)); break;
        case 2: store(data, static_cast<i16>(x)); break;
        case 4: store(data, static_cast<i32>(x)); break;
        default: store(data, static_cast<i64>(x)); break;
        }
        return true;
    }
    case Kind::UInt: {
        unsigned long long x = std::strtoull(s, &end, 10);
        if (end == s || *end) {
            const auto it = std::find(v.enumNames.begin(), v.enumNames.end(), text); // MEALKIT_ENUM 이름
            if (it == v.enumNames.end()) return false;
            x = static_cast<unsigned long long>(it - v.enumNames.begin());
        }
        switch (v.size) {
        case 1: store(data, static_cast<u8>(x)); break;
        case 2: store(data, static_cast<u16>(x)); break;
        case 4: store(data, static_cast<u32>(x)); break;
        default: store(data, static_cast<u64>(x)); break;
        }
        return true;
    }
    case Kind::Float: {
        f32 f = 0.0f;
        if (!parseFloats(s, &f, 1)) return false;
        store(data, f);
        return true;
    }
    case Kind::Vec3: {
        f32 f[3];
        if (!parseFloats(s, f, 3)) return false;
        store(data, Vec3{f[0], f[1], f[2]});
        return true;
    }
    case Kind::Quat: {
        f32 f[4];
        if (!parseFloats(s, f, 4)) return false;
        store(data, Quat{f[0], f[1], f[2], f[3]});
        return true;
    }
    case Kind::String: *static_cast<std::string*>(data) = text; return true;
    case Kind::RowRef: { // 행 id (번호는 data::resolveRows가 채운다)
        const RowRefView r = rowRefOf(data);
        *r.id = text;
        *r.index = kNoRow;
        return true;
    }
    case Kind::Ticks: { // 초
        f32 seconds = 0.0f;
        if (!parseFloats(s, &seconds, 1)) return false;
        store(data, Ticks::seconds(seconds));
        return true;
    }
    case Kind::Color: { // #RRGGBB 또는 #RRGGBBAA
        if (text.size() != 7 && text.size() != 9) return false;
        if (text[0] != '#') return false;
        const unsigned long hex = std::strtoul(s + 1, &end, 16);
        if (*end) return false;
        const u32 h = static_cast<u32>(hex);
        const bool alpha = text.size() == 9;
        const u32 r = alpha ? h >> 24 : h >> 16, g = (alpha ? h >> 16 : h >> 8) & 0xFF, b = (alpha ? h >> 8 : h) & 0xFF;
        const u32 a = alpha ? h & 0xFF : 0xFF;
        store(data, Color::rgb(static_cast<u8>(r & 0xFF), static_cast<u8>(g), static_cast<u8>(b), static_cast<u8>(a)));
        return true;
    }
    case Kind::Id: {
        // "<slot> <generation>" (sscanf_s는 MSVC 전용, sscanf는 MSVC가 C4996 경고)
        const unsigned long slot = std::strtoul(s, &end, 10);
        if (end == s) return false;
        const char* rest = end;
        const unsigned long generation = std::strtoul(rest, &end, 10);
        if (end == rest) return false;
        const u32 ids[2] = {static_cast<u32>(slot), static_cast<u32>(generation)};
        std::memcpy(data, ids, sizeof(ids));
        return true;
    }
    default: return false;
    }
}

void toTextValue(const ValueInfo& v, const void* data, const std::string& path, TextFields& out);

void toTextStruct(const TypeInfo& type, const void* data, const std::string& prefix, TextFields& out) {
    for (const FieldInfo& f : type.fields) toTextValue(*f.value, at(data, f.offset), prefix + f.name, out);
}

void toTextValue(const ValueInfo& v, const void* data, const std::string& path, TextFields& out) {
    switch (v.kind) {
    case Kind::Struct: toTextStruct(*v.type, data, path + ".", out); break;
    case Kind::Array:
        for (u32 i = 0; i < v.count; ++i)
            toTextValue(*v.element, at(data, i * v.element->size), path + "[" + std::to_string(i) + "]", out);
        break;
    case Kind::Container: break; // 매치 중에 생기는 객체: 텍스트로 쓰지 않는다
    default: out.emplace_back(path, formatScalar(v, data)); break;
    }
}

// "a.b[2].c" 를 따라가 값 하나를 찾는다
bool resolve(const TypeInfo& type, void* data, const char* path, const ValueInfo*& outValue, void*& outData) {
    const char* end = path;
    while (*end && *end != '.' && *end != '[') ++end;
    const std::string name(path, end);
    const FieldInfo* field = type.field(name.c_str());
    if (!field) return false;
    const ValueInfo* v = field->value;
    void* p = at(data, field->offset);
    while (*end == '[') {
        if (v->kind != Kind::Array) return false;
        char* close = nullptr;
        const unsigned long index = std::strtoul(end + 1, &close, 10);
        if (!close || *close != ']' || index >= v->count) return false;
        p = at(p, static_cast<u32>(index) * v->element->size);
        v = v->element;
        end = close + 1;
    }
    if (*end == '.') {
        if (v->kind != Kind::Struct) return false;
        return resolve(*v->type, p, end + 1, outValue, outData);
    }
    if (*end) return false;
    outValue = v;
    outData = p;
    return true;
}

void diffValue(const ValueInfo& v, const void* a, const void* b, const std::string& path, std::vector<std::string>& out,
               u32 maxLines);

void diffFields(const TypeInfo& type, const void* a, const void* b, const std::string& prefix,
                std::vector<std::string>& out, u32 maxLines) {
    for (const FieldInfo& f : type.fields) {
        if (out.size() >= maxLines) return;
        diffValue(*f.value, at(a, f.offset), at(b, f.offset), prefix + f.name, out, maxLines);
    }
}

void diffValue(const ValueInfo& v, const void* a, const void* b, const std::string& path, std::vector<std::string>& out,
               u32 maxLines) {
    if (out.size() >= maxLines) return;
    switch (v.kind) {
    case Kind::Struct: diffFields(*v.type, a, b, path + ".", out, maxLines); break;
    case Kind::Array:
        for (u32 i = 0; i < v.count && out.size() < maxLines; ++i)
            diffValue(*v.element, at(a, i * v.element->size), at(b, i * v.element->size),
                      path + "[" + std::to_string(i) + "]", out, maxLines);
        break;
    case Kind::Container: {
        const ContainerOps& ops = *v.ops;
        const size_t before = out.size();
        const u32 capA = ops.capacity(a), capB = ops.capacity(b);
        if (capA != capB) {
            out.push_back(path + ": capacity " + std::to_string(capA) + " -> " + std::to_string(capB));
            return;
        }
        for (u32 slot = 0; slot < capA && out.size() < maxLines; ++slot) {
            const bool la = ops.live(a, slot), lb = ops.live(b, slot);
            const std::string itemPath = path + "#" + std::to_string(slot);
            if (la != lb) out.push_back(itemPath + (la ? ": alive -> free" : ": free -> alive"));
            else if (la) diffValue(*v.element, ops.item(a, slot), ops.item(b, slot), itemPath, out, maxLines);
        }
        if (out.size() == before) {
            Hasher ha, hb;
            ops.hashSlots(a, ha);
            ops.hashSlots(b, hb);
            if (ha.value != hb.value) out.push_back(path + ": slot bookkeeping (generations or free list) differs");
        }
        break;
    }
    case Kind::String:
        if (*static_cast<const std::string*>(a) != *static_cast<const std::string*>(b))
            out.push_back(path + ": " + formatScalar(v, a) + " -> " + formatScalar(v, b));
        break;
    case Kind::RowRef:
        if (formatScalar(v, a) != formatScalar(v, b))
            out.push_back(path + ": " + formatScalar(v, a) + " -> " + formatScalar(v, b));
        break;
    default:
        if (std::memcmp(a, b, v.size) != 0)
            out.push_back(path + ": " + formatScalar(v, a) + " -> " + formatScalar(v, b));
        break;
    }
}

void saveValue(const ValueInfo& v, const void* data, ByteWriter& w) {
    switch (v.kind) {
    case Kind::Struct: saveStruct(*v.type, data, w); break;
    case Kind::Array:
        for (u32 i = 0; i < v.count; ++i) saveValue(*v.element, at(data, i * v.element->size), w);
        break;
    case Kind::Container: v.ops->save(data, w); break;
    case Kind::String: {
        const std::string& text = *static_cast<const std::string*>(data);
        w.value(static_cast<u32>(text.size()));
        w.bytes(text.data(), text.size());
        break;
    }
    case Kind::RowRef: {
        const RowRefView r = rowRefOf(const_cast<void*>(data));
        w.value(static_cast<u32>(r.id->size()));
        w.bytes(r.id->data(), r.id->size());
        w.value(*r.index);
        break;
    }
    default: w.bytes(data, v.size); break;
    }
}

bool loadValue(const ValueInfo& v, void* data, ByteReader& r) {
    switch (v.kind) {
    case Kind::Struct: return loadStruct(*v.type, data, r);
    case Kind::Array:
        for (u32 i = 0; i < v.count; ++i)
            if (!loadValue(*v.element, at(data, i * v.element->size), r)) return false;
        return true;
    case Kind::Container: return v.ops->load(data, r);
    case Kind::String: {
        u32 size = 0;
        if (!r.value(size)) return false;
        std::string& text = *static_cast<std::string*>(data);
        text.resize(size);
        return r.bytes(text.data(), size);
    }
    case Kind::RowRef: {
        const RowRefView ref = rowRefOf(data);
        u32 size = 0;
        if (!r.value(size)) return false;
        ref.id->resize(size);
        return r.bytes(ref.id->data(), size) && r.value(*ref.index);
    }
    default: return r.bytes(data, v.size);
    }
}

} // namespace

const FieldInfo* TypeInfo::field(const char* fieldName) const {
    for (const FieldInfo& f : fields)
        if (std::strcmp(f.name, fieldName) == 0) return &f;
    return nullptr;
}

void hashValue(const ValueInfo& v, const void* data, Hasher& h, u32 skipFlags) {
    switch (v.kind) {
    case Kind::Struct: hashStruct(*v.type, data, h, skipFlags); break;
    case Kind::Array:
        for (u32 i = 0; i < v.count; ++i) hashValue(*v.element, at(data, i * v.element->size), h, skipFlags);
        break;
    case Kind::Container: {
        v.ops->hashSlots(data, h);
        const u32 capacity = v.ops->capacity(data);
        for (u32 slot = 0; slot < capacity; ++slot)
            if (v.ops->live(data, slot)) hashValue(*v.element, v.ops->item(data, slot), h, skipFlags);
        break;
    }
    case Kind::String: {
        const std::string& text = *static_cast<const std::string*>(data);
        h.add(static_cast<u64>(text.size()));
        h.bytes(text.data(), text.size());
        break;
    }
    case Kind::RowRef: { // id가 정체성이다 (행 번호는 같은 표에서 저절로 같다)
        const std::string& id = *rowRefOf(const_cast<void*>(data)).id;
        h.add(static_cast<u64>(id.size()));
        h.bytes(id.data(), id.size());
        break;
    }
    default: h.bytes(data, v.size); break; // 스칼라·Vec3·Quat·Id는 패딩이 없다
    }
}

void hashStruct(const TypeInfo& type, const void* data, Hasher& h, u32 skipFlags) {
    for (const FieldInfo& f : type.fields)
        if (!(f.flags & skipFlags)) hashValue(*f.value, at(data, f.offset), h, skipFlags);
}

void saveStruct(const TypeInfo& type, const void* data, ByteWriter& w) {
    for (const FieldInfo& f : type.fields) saveValue(*f.value, at(data, f.offset), w);
}

bool loadStruct(const TypeInfo& type, void* data, ByteReader& r) {
    for (const FieldInfo& f : type.fields)
        if (!loadValue(*f.value, at(data, f.offset), r)) return false;
    return true;
}

std::string formatScalar(const ValueInfo& v, const void* data) {
    char text[96] = {};
    switch (v.kind) {
    case Kind::Bool: return *static_cast<const bool*>(data) ? "true" : "false";
    case Kind::Int: {
        const long long x = readInt(data, v.size);
        if (x >= 0 && static_cast<size_t>(x) < v.enumNames.size()) return v.enumNames[static_cast<size_t>(x)];
        snprintf(text, sizeof(text), "%lld", x);
        break;
    }
    case Kind::UInt: {
        const unsigned long long x = readUInt(data, v.size);
        if (x < v.enumNames.size()) return v.enumNames[static_cast<size_t>(x)];
        snprintf(text, sizeof(text), "%llu", x);
        break;
    }
    case Kind::Float: text::writeFloat(text, sizeof(text), *static_cast<const f32*>(data)); break;
    case Kind::Vec3: {
        const Vec3& p = *static_cast<const Vec3*>(data);
        const f32 v[3] = {p.x, p.y, p.z};
        text::writeFloats(text, sizeof(text), v, 3);
        break;
    }
    case Kind::Quat: {
        const Quat& q = *static_cast<const Quat*>(data);
        const f32 v[4] = {q.x, q.y, q.z, q.w};
        text::writeFloats(text, sizeof(text), v, 4);
        break;
    }
    case Kind::String: return *static_cast<const std::string*>(data);
    case Kind::RowRef: return *rowRefOf(const_cast<void*>(data)).id;
    case Kind::Ticks: snprintf(text, sizeof(text), "%.6g", static_cast<const Ticks*>(data)->toSeconds()); break; // 다시 읽으면 같은 틱
    case Kind::Color: {
        const u32 c = static_cast<const Color*>(data)->value;
        const u32 r = c & 0xFF, g = (c >> 8) & 0xFF, b = (c >> 16) & 0xFF, a = c >> 24;
        if (a == 0xFF) snprintf(text, sizeof(text), "#%02X%02X%02X", r, g, b);
        else snprintf(text, sizeof(text), "#%02X%02X%02X%02X", r, g, b, a);
        break;
    }
    case Kind::Id: {
        u32 ids[2];
        std::memcpy(ids, data, sizeof(ids));
        snprintf(text, sizeof(text), "%u %u", ids[0], ids[1]);
        break;
    }
    default: break;
    }
    return text;
}

void toText(const TypeInfo& type, const void* data, TextFields& out) { toTextStruct(type, data, "", out); }

std::vector<std::string> splitEnumNames(const char* text) {
    std::vector<std::string> names;
    std::string current;
    for (const char* p = text;; ++p) {
        if (*p == ',' || *p == '\0') {
            const size_t a = current.find_first_not_of(" \t\n\r");
            const size_t b = current.find_last_not_of(" \t\n\r");
            names.push_back(a == std::string::npos ? std::string() : current.substr(a, b - a + 1));
            current.clear();
            if (!*p) break;
        } else {
            current += *p;
        }
    }
    return names;
}

RowRefView rowRefOf(void* data) {
    auto* r = static_cast<RowRef<int>*>(data); // 행 타입과 무관하게 배치가 같다 (id, index)
    return {&r->id, &r->index};
}

bool resolvePath(const TypeInfo& type, void* data, const std::string& path, const ValueInfo*& outValue,
                 void*& outData) {
    return resolve(type, data, path.c_str(), outValue, outData);
}

bool setFromText(const TypeInfo& type, void* data, const std::string& path, const std::string& text) {
    const ValueInfo* v = nullptr;
    void* p = nullptr;
    if (!resolve(type, data, path.c_str(), v, p)) return false;
    if (v->kind == Kind::Struct || v->kind == Kind::Array || v->kind == Kind::Container) return false;
    return parseScalar(*v, p, text);
}

void fromText(const TypeInfo& type, void* data, const TextFields& fields, std::string* warnings) {
    for (const auto& [path, text] : fields) {
        if (setFromText(type, data, path, text)) continue;
        if (warnings) *warnings += (warnings->empty() ? "" : ", ") + path;
    }
}

bool saveTextFile(const TypeInfo& type, const void* data, const std::string& path, const text::Format& format, const char* header) {
    TextFields fields;
    toText(type, data, fields);
    std::string out;
    if (header && header[0]) out += std::string("# ") + header + "\n";
    out += text::formatLine(format) + "\n";
    for (const auto& [key, value] : fields) out += key + " " + value + "\n";
    if (!writeTextFile(path, out)) {
        MEALKIT_ERROR("Cannot write %s", path.c_str());
        return false;
    }
    return true;
}

bool loadTextFile(const TypeInfo& type, void* data, const std::string& path, const text::Format& format, std::string* error) {
    std::string content;
    if (!readTextFile(path, content)) {
        if (error) *error = "cannot open file";
        return false;
    }
    size_t start = 0;
    int lineNo = 0;
    while (start < content.size()) {
        size_t end = content.find('\n', start);
        if (end == std::string::npos) end = content.size();
        std::string line = content.substr(start, end - start);
        start = end + 1;
        ++lineNo;
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) line.pop_back();
        size_t first = line.find_first_not_of(" \t");
        if (first == std::string::npos || line[first] == '#') continue;
        line = line.substr(first);
        u32 version = 1; // 지금은 버전 1뿐이라 migrate가 없다
        if (text::readFormatLine(line, format, path, version)) continue;
        const size_t space = line.find(' ');
        const std::string key = line.substr(0, space);
        const std::string value =
            space == std::string::npos ? std::string() : line.substr(line.find_first_not_of(' ', space));
        if (!setFromText(type, data, key, value))
            MEALKIT_WARN("%s:%d: unknown key or invalid value '%s' ignored", path.c_str(), lineNo, key.c_str());
    }
    return true;
}

void diffStruct(const TypeInfo& type, const void* a, const void* b, std::vector<std::string>& out, u32 maxLines) {
    diffFields(type, a, b, "", out, maxLines);
}

} // namespace mealkit::reflect

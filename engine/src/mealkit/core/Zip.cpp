#include "mealkit/core/Zip.h"

#include "mealkit/platform/Paths.h"

#include <cstdint>
#include <cstdio>

namespace mealkit {
namespace {

constexpr u32 kLocalSig = 0x04034b50, kCentralSig = 0x02014b50, kEndSig = 0x06054b50;
constexpr u16 kUtf8Names = 0x0800;
constexpr u16 kDosDate = (46u << 9) | (1u << 5) | 1u; // 2026-01-01 고정 (같은 내용이면 같은 바이트)

void put(std::vector<u8>& out, u32 v, int bytes) {
    for (int i = 0; i < bytes; ++i) out.push_back(static_cast<u8>(v >> (8 * i)));
}
u32 get(const u8* p, int bytes) {
    u32 v = 0;
    for (int i = 0; i < bytes; ++i) v |= static_cast<u32>(p[i]) << (8 * i);
    return v;
}

} // namespace

u32 crc32(const u8* data, size_t size) {
    static u32 table[256] = {};
    if (!table[1])
        for (u32 i = 0; i < 256; ++i) {
            u32 c = i;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            table[i] = c;
        }
    u32 c = 0xFFFFFFFFu;
    for (size_t i = 0; i < size; ++i) c = table[(c ^ data[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

bool writeZip(const std::string& path, const std::vector<ZipEntry>& entries) {
    std::vector<u8> out, central;
    for (const ZipEntry& e : entries) {
        const u32 offset = static_cast<u32>(out.size());
        const u32 crc = crc32(e.data.data(), e.data.size());
        const u32 size = static_cast<u32>(e.data.size());
        const u32 nameLength = static_cast<u32>(e.name.size());
        put(out, kLocalSig, 4);
        put(out, 20, 2);
        put(out, kUtf8Names, 2);
        put(out, 0, 2); // stored
        put(out, 0, 2);
        put(out, kDosDate, 2);
        put(out, crc, 4);
        put(out, size, 4);
        put(out, size, 4);
        put(out, nameLength, 2);
        put(out, 0, 2);
        out.insert(out.end(), e.name.begin(), e.name.end());
        out.insert(out.end(), e.data.begin(), e.data.end());

        put(central, kCentralSig, 4);
        put(central, 20, 2);
        put(central, 20, 2);
        put(central, kUtf8Names, 2);
        put(central, 0, 2);
        put(central, 0, 2);
        put(central, kDosDate, 2);
        put(central, crc, 4);
        put(central, size, 4);
        put(central, size, 4);
        put(central, nameLength, 2);
        put(central, 0, 2); // extra
        put(central, 0, 2); // comment
        put(central, 0, 2); // disk
        put(central, 0, 2); // internal attributes
        put(central, 0, 4); // external attributes
        put(central, offset, 4);
        central.insert(central.end(), e.name.begin(), e.name.end());
    }
    const u32 centralOffset = static_cast<u32>(out.size());
    out.insert(out.end(), central.begin(), central.end());
    put(out, kEndSig, 4);
    put(out, 0, 4); // disk 번호
    put(out, static_cast<u32>(entries.size()), 2);
    put(out, static_cast<u32>(entries.size()), 2);
    put(out, static_cast<u32>(central.size()), 4);
    put(out, centralOffset, 4);
    put(out, 0, 2);

    FILE* f = openFile(path, "wb");
    if (!f) return false;
    const bool ok = std::fwrite(out.data(), 1, out.size(), f) == out.size();
    return std::fclose(f) == 0 && ok;
}

bool readZip(const std::string& path, std::vector<ZipEntry>& out, std::string& error) {
    out.clear();
    FILE* f = openFile(path, "rb");
    if (!f) {
        error = "cannot open " + path;
        return false;
    }
    std::vector<u8> bytes;
    u8 buffer[65536];
    for (size_t n; (n = std::fread(buffer, 1, sizeof(buffer), f)) > 0;) bytes.insert(bytes.end(), buffer, buffer + n);
    std::fclose(f);

    // 끝 레코드는 뒤에서 찾는다 (뒤에 주석이 붙을 수 있다)
    size_t end = bytes.size() >= 22 ? bytes.size() - 22 : SIZE_MAX;
    for (; end != SIZE_MAX; --end)
        if (get(&bytes[end], 4) == kEndSig) break;
    if (end == SIZE_MAX) {
        error = "not a zip file";
        return false;
    }
    const u32 count = get(&bytes[end + 10], 2);
    size_t p = get(&bytes[end + 16], 4);
    for (u32 i = 0; i < count; ++i) {
        if (p + 46 > bytes.size() || get(&bytes[p], 4) != kCentralSig) {
            error = "broken zip directory";
            return false;
        }
        const u32 method = get(&bytes[p + 10], 2);
        const u32 crc = get(&bytes[p + 16], 4);
        const u32 size = get(&bytes[p + 20], 4);
        const u32 nameLength = get(&bytes[p + 28], 2);
        const u32 skip = nameLength + get(&bytes[p + 30], 2) + get(&bytes[p + 32], 2);
        const size_t local = get(&bytes[p + 42], 4);
        if (p + 46 + skip > bytes.size() || local + 30 > bytes.size()) {
            error = "broken zip directory";
            return false;
        }
        ZipEntry e;
        e.name.assign(reinterpret_cast<const char*>(&bytes[p + 46]), nameLength);
        p += 46 + skip;
        if (method != 0) {
            error = "compressed entry " + e.name + " (only stored zip files are read)";
            return false;
        }
        const size_t data = local + 30 + get(&bytes[local + 26], 2) + get(&bytes[local + 28], 2);
        if (data + size > bytes.size() || crc32(&bytes[data], size) != crc) {
            error = "broken entry " + e.name;
            return false;
        }
        e.data.assign(bytes.begin() + data, bytes.begin() + data + size);
        out.push_back(std::move(e));
    }
    return true;
}

} // namespace mealkit

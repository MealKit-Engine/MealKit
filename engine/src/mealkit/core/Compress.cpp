#include "mealkit/core/Compress.h"

#include <lz4.h>

namespace mealkit {

std::vector<u8> compressBytes(const u8* data, u32 size) {
    std::vector<u8> out(static_cast<size_t>(LZ4_compressBound(static_cast<int>(size))));
    const int n = LZ4_compress_default(reinterpret_cast<const char*>(data), reinterpret_cast<char*>(out.data()),
                                       static_cast<int>(size), static_cast<int>(out.size()));
    out.resize(n > 0 ? static_cast<size_t>(n) : 0);
    return out;
}

bool decompressBytes(const u8* data, u32 size, u32 originalSize, std::vector<u8>& out) {
    out.resize(originalSize);
    const int n = LZ4_decompress_safe(reinterpret_cast<const char*>(data), reinterpret_cast<char*>(out.data()),
                                      static_cast<int>(size), static_cast<int>(originalSize));
    return n >= 0 && static_cast<u32>(n) == originalSize;
}

} // namespace mealkit

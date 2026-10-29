#pragma once

#include "mealkit/core/Types.h"

#include <vector>

// Byte compression (LZ4, third_party/lz4): world snapshots sent on late join/reconnect. Chosen for fast decompression (join time over ratio).
namespace mealkit {

// Compressed bytes (the original size is sent separately)
std::vector<u8> compressBytes(const u8* data, u32 size);
// The original size must be known. false for corrupt data or a size mismatch
bool decompressBytes(const u8* data, u32 size, u32 originalSize, std::vector<u8>& out);

} // namespace mealkit

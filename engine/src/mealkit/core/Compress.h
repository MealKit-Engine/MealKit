#pragma once

#include "mealkit/core/Types.h"

#include <vector>

// 바이트 압축 (LZ4, third_party/lz4): 중간 참가·재접속 때 보내는 월드 스냅샷. 빠르게 풀리는 쪽을 고른다(압축률보다 참가 시간).
namespace mealkit {

// 압축한 바이트 (원래 크기는 따로 보낸다)
std::vector<u8> compressBytes(const u8* data, u32 size);
// 원래 크기를 알아야 한다. 깨진 데이터거나 크기가 다르면 false
bool decompressBytes(const u8* data, u32 size, u32 originalSize, std::vector<u8>& out);

} // namespace mealkit

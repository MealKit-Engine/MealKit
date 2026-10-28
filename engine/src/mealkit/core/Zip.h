#pragma once

#include "mealkit/core/Types.h"

#include <string>
#include <vector>

// zip 파일 (압축 없음, "stored"): 버그 리포트 묶음(game/BugReport.h)처럼 여러 파일을 하나로 넘길 때.
// 탐색기·7-Zip으로 열 수 있다. 읽기는 압축 없는 항목만 (다른 도구로 다시 압축한 zip은 읽지 못한다).
namespace mealkit {

struct ZipEntry {
    std::string name; // zip 안의 경로 ('/'로 나눈다, UTF-8)
    std::vector<u8> data;
};

u32 crc32(const u8* data, size_t size);

bool writeZip(const std::string& path, const std::vector<ZipEntry>& entries);
// 항목 전부. 압축된 항목이 있거나 깨진 파일이면 false (error에 이유)
bool readZip(const std::string& path, std::vector<ZipEntry>& out, std::string& error);

} // namespace mealkit

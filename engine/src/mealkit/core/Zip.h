#pragma once

#include "mealkit/core/Types.h"

#include <string>
#include <vector>

// zip file (no compression, "stored"): for passing multiple files as one, like bug report bundles (game/BugReport.h).
// Opens in Explorer/7-Zip. Reading supports only uncompressed entries (zips recompressed by other tools can't be read).
namespace mealkit {

struct ZipEntry {
    std::string name; // path inside the zip (split by '/', UTF-8)
    std::vector<u8> data;
};

u32 crc32(const u8* data, size_t size);

bool writeZip(const std::string& path, const std::vector<ZipEntry>& entries);
// All entries. false if there are compressed entries or the file is corrupt (reason in error)
bool readZip(const std::string& path, std::vector<ZipEntry>& out, std::string& error);

} // namespace mealkit

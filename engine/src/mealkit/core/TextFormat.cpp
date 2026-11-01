#include "mealkit/core/TextFormat.h"

#include "mealkit/core/Log.h"

#include <mkcontract/Text.h>

#include <charconv>
#include <cstdlib>

namespace mealkit::text {

std::string formatLine(const Format& format) { return mkcontract::formatLine(format.kind, format.version); }

bool readFormatLine(std::string_view line, const Format& expected, const std::string& path, u32& version) {
    mkcontract::FormatLine f;
    switch (mkcontract::parseFormatLine(line, f)) {
    case mkcontract::FormatParse::NotFormat: return false;
    case mkcontract::FormatParse::Malformed:
        MEALKIT_WARN("%s: expected \"@format <kind> <version>\", got \"%.*s\" (ignored)", path.c_str(), static_cast<int>(line.size()), line.data());
        return true;
    case mkcontract::FormatParse::Ok: break;
    }
    version = f.version;
    if (f.kind != expected.kind)
        MEALKIT_WARN("%s: this is a '%s' file, expected '%s'", path.c_str(), f.kind.c_str(), expected.kind);
    else if (version > expected.version)
        MEALKIT_WARN("%s: written in %s format %u, this engine reads up to %u - unknown keys are ignored (update the engine)", path.c_str(),
                     expected.kind, version, expected.version);
    return true;
}

char* writeFloats(char* out, size_t capacity, const f32* values, u32 count) {
    char* p = out;
    char* const end = out + capacity - 1; // room for the null terminator
    for (u32 i = 0; i < count && p < end; ++i) {
        if (i > 0) *p++ = ' ';
        const f32 v = values[i] == 0.0f ? 0.0f : values[i]; // -0 becomes 0
        const std::to_chars_result r = std::to_chars(p, end, v);
        if (r.ec != std::errc{}) break;
        p = r.ptr;
    }
    *p = '\0';
    return p;
}

} // namespace mealkit::text

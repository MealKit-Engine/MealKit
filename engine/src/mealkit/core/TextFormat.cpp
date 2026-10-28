#include "mealkit/core/TextFormat.h"

#include "mealkit/core/Log.h"

#include <charconv>
#include <cstdlib>

namespace mealkit::text {

std::string formatLine(const Format& format) { return std::string("@format ") + format.kind + " " + std::to_string(format.version); }

bool readFormatLine(std::string_view line, const Format& expected, const std::string& path, u32& version) {
    if (line.empty() || line[0] != '@') return false;
    // "@format <종류> <N>"
    std::string words[3];
    u32 count = 0;
    for (size_t a = 0; a < line.size() && count < 4;) {
        while (a < line.size() && (line[a] == ' ' || line[a] == '\t')) ++a;
        if (a >= line.size()) break;
        size_t b = a;
        while (b < line.size() && line[b] != ' ' && line[b] != '\t') ++b;
        if (count < 3) words[count] = std::string(line.substr(a, b - a));
        ++count;
        a = b;
    }
    char* end = nullptr;
    const unsigned long n = count == 3 ? std::strtoul(words[2].c_str(), &end, 10) : 0;
    if (count != 3 || words[0] != "@format" || n == 0 || !end || *end != '\0') {
        MEALKIT_WARN("%s: expected \"@format <kind> <version>\", got \"%.*s\" (ignored)", path.c_str(), static_cast<int>(line.size()), line.data());
        return true;
    }
    version = static_cast<u32>(n);
    if (words[1] != expected.kind)
        MEALKIT_WARN("%s: this is a '%s' file, expected '%s'", path.c_str(), words[1].c_str(), expected.kind);
    else if (version > expected.version)
        MEALKIT_WARN("%s: written in %s format %u, this engine reads up to %u - unknown keys are ignored (update the engine)", path.c_str(),
                     expected.kind, version, expected.version);
    return true;
}

char* writeFloats(char* out, size_t capacity, const f32* values, u32 count) {
    char* p = out;
    char* const end = out + capacity - 1; // 널 문자 자리
    for (u32 i = 0; i < count && p < end; ++i) {
        if (i > 0) *p++ = ' ';
        const f32 v = values[i] == 0.0f ? 0.0f : values[i]; // -0은 0으로
        const std::to_chars_result r = std::to_chars(p, end, v);
        if (r.ec != std::errc{}) break;
        p = r.ptr;
    }
    *p = '\0';
    return p;
}

} // namespace mealkit::text

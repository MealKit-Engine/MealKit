// Checks that all characters in the string tables exist in the fonts (ROADMAP stage 1 "Localization string tables": glyphs are baked at runtime).
//   mealkit_stringcheck <stamp> --fonts <assets/fonts> <assets/strings/*.txt ...>
// mealkit_add_game calls this before building the game. It's enough if a character is in any one of the fonts (.ttf, .otf, first font of .ttc, including subfolders).
// If there are no fonts at all, nothing is checked (projects without game UI yet). Missing characters are reported in MSVC format and fail.
// The file format is the same as mealkit/loc/Strings.cpp ("key = string", # comments). Only the reading is duplicated here so it builds without the engine library.
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

struct Font {
    std::string path;
    std::vector<unsigned char> data;
    stbtt_fontinfo info{};
};

bool readFile(const fs::path& path, std::vector<unsigned char>& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    return true;
}

// UTF-8 → code points (bad bytes become U+FFFD)
std::vector<unsigned> decode(const std::string& s) {
    std::vector<unsigned> out;
    for (size_t i = 0; i < s.size();) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        unsigned cp = 0xFFFD;
        size_t n = 1;
        if (c < 0x80) cp = c;
        else if ((c >> 5) == 0x6 && i + 1 < s.size()) { cp = ((c & 0x1Fu) << 6) | (s[i + 1] & 0x3Fu); n = 2; }
        else if ((c >> 4) == 0xE && i + 2 < s.size()) { cp = ((c & 0x0Fu) << 12) | ((s[i + 1] & 0x3Fu) << 6) | (s[i + 2] & 0x3Fu); n = 3; }
        else if ((c >> 3) == 0x1E && i + 3 < s.size()) {
            cp = ((c & 0x07u) << 18) | ((s[i + 1] & 0x3Fu) << 12) | ((s[i + 2] & 0x3Fu) << 6) | (s[i + 3] & 0x3Fu);
            n = 4;
        }
        out.push_back(cp);
        i += n;
    }
    return out;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: mealkit_stringcheck <stamp> --fonts <dir> <strings.txt ...>\n");
        return 2;
    }
    const std::string stamp = argv[1];
    std::string fontDir;
    std::vector<std::string> files;
    for (int i = 2; i < argc; ++i) {
        if (std::strcmp(argv[i], "--fonts") == 0 && i + 1 < argc) fontDir = argv[++i];
        else files.push_back(argv[i]);
    }

    std::vector<Font> fonts;
    std::error_code ec;
    if (!fontDir.empty() && fs::is_directory(fontDir, ec)) {
        for (fs::recursive_directory_iterator it(fontDir, ec), end; !ec && it != end; it.increment(ec)) {
            if (!it->is_regular_file(ec)) continue;
            std::string ext = it->path().extension().string();
            for (char& c : ext) c = static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
            if (ext != ".ttf" && ext != ".otf" && ext != ".ttc") continue;
            Font f;
            f.path = it->path().string();
            if (!readFile(it->path(), f.data) || f.data.empty()) continue;
            const int offset = stbtt_GetFontOffsetForIndex(f.data.data(), 0);
            if (offset < 0 || !stbtt_InitFont(&f.info, f.data.data(), offset)) {
                std::fprintf(stderr, "%s: warning STR002: cannot read this font\n", f.path.c_str());
                continue;
            }
            fonts.push_back(std::move(f)); // the byte buffer stays put even when the vector moves, so info's pointers stay valid
        }
    }

    int problems = 0;
    if (!fonts.empty()) {
        for (const std::string& file : files) {
            std::ifstream in{fs::path(file)};
            if (!in) continue;
            std::map<unsigned, unsigned> missing; // code point → first line it appeared on
            std::string line;
            unsigned lineNo = 0;
            while (std::getline(in, line)) {
                ++lineNo;
                size_t start = 0;
                if (lineNo == 1 && line.size() >= 3 && static_cast<unsigned char>(line[0]) == 0xEF) start = 3; // BOM
                const size_t first = line.find_first_not_of(" \t\r", start);
                if (first == std::string::npos || line[first] == '#' || line[first] == '@') continue; // comments, format line "@format strings 1"
                const size_t eq = line.find('=', first);
                if (eq == std::string::npos) continue;
                for (unsigned cp : decode(line.substr(eq + 1))) {
                    if (cp < 0x20 || cp == '\r') continue;
                    bool found = false;
                    for (const Font& f : fonts) found = found || stbtt_FindGlyphIndex(&f.info, static_cast<int>(cp)) != 0;
                    if (!found && !missing.count(cp)) missing[cp] = lineNo;
                }
            }
            for (const auto& [cp, at] : missing) {
                std::fprintf(stderr, "%s(%u): error STR001: U+%04X is not in any font in %s - add a font that has it\n",
                             file.c_str(), at, cp, fontDir.c_str());
                ++problems;
            }
        }
    }
    if (problems) {
        std::fprintf(stderr, "StringCheck: %d character(s) missing from the fonts\n", problems);
        return 1;
    }
    std::ofstream(fs::path(stamp)) << "ok\n";
    return 0;
}

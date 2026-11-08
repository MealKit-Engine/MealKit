#pragma once

#include "mealkit/core/Types.h"

#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Game string tables (docs/CODING_MODEL.md "Game text and languages"): game code and UI use keys instead of text.
//   assets/strings/ko.txt, en.txt ...   one "key = string" per line (# comments, \n for newlines, \\ for backslash)
//     lobby.waiting = Waiting for host ({count} waiting)
//   ctx.strings.get("lobby.waiting"), ctx.strings.format("lobby.waiting", {{"count", "3"}})
// Keys missing from the chosen language come from the default language (project setting language) table; if missing there too, the key name is shown as is with one warning per key.
// Multi-style text uses numbered tags: "Score <1>{score}</1> pts" (tags are interpreted by the UI; the string table returns them as is).
// Whether all characters in the tables exist in the fonts in assets/fonts is checked at build time (mealkit_stringcheck). Unrelated to the content hash.
namespace mealkit::loc {

inline constexpr const char* kStringsFolder = "strings"; // relative to assets

class Strings {
public:
    Strings();
    ~Strings();
    Strings(const Strings&) = delete;
    Strings& operator=(const Strings&) = delete;

    // language: language to show (file name, "ko"), fallback: default language. Reads the files. update checks file times every 0.25 s and rereads.
    void load(std::string language, std::string fallback);
    void update(f64 dt);
    const std::string& language() const;
    // Languages present in assets/strings (language picker in the settings screen)
    std::vector<std::string> languages() const;

    // String for the key. Key name if missing (one warning). Valid until the file is reread (don't keep it)
    const std::string& get(std::string_view key) const;
    bool has(std::string_view key) const;
    // Replaces {name} placeholders with values. Unknown names are left as is
    std::string format(std::string_view key, std::initializer_list<std::pair<std::string_view, std::string>> args) const;

private:
    struct Impl;
    Impl* m;
};

// Reads one "key = string" file (tools/checks use the same function). Line numbers start at 1. Malformed lines go to errors.
struct StringEntry {
    std::string key;
    std::string value;
    u32 line = 0;
};
bool readStringsFile(const std::string& path, std::vector<StringEntry>& out, std::string* errors = nullptr);
bool validStringKey(std::string_view key); // letters/digits/._-, first character a letter

} // namespace mealkit::loc

#include "mealkit/loc/Strings.h"

#include "mealkit/core/Log.h"
#include "mealkit/core/TextFormat.h"
#include "mealkit/platform/Paths.h"

#include <algorithm>
#include <deque>

namespace mealkit::loc {
namespace {

constexpr f64 kFileCheckSeconds = 0.25;

std::string trim(std::string_view s) {
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r')) ++a;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r')) --b;
    return std::string(s.substr(a, b - a));
}

// \n → newline, \\ → backslash (other \ left as is)
std::string unescape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size() && (s[i + 1] == 'n' || s[i + 1] == '\\')) {
            out += s[i + 1] == 'n' ? '\n' : '\\';
            ++i;
        } else {
            out += s[i];
        }
    }
    return out;
}

struct Table {
    std::string language;
    std::string path;
    unsigned long long time = 0;
    std::vector<std::pair<std::string, unsigned long long>> modules; // source modules' tables read under this one (path, time)
    std::vector<StringEntry> entries; // sorted by key

    const std::string* find(std::string_view key) const {
        auto it = std::lower_bound(entries.begin(), entries.end(), key,
                                   [](const StringEntry& e, std::string_view k) { return e.key < k; });
        return it != entries.end() && it->key == key ? &it->value : nullptr;
    }
};

std::string tablePath(const std::string& language) {
    return findAsset(std::string("assets/") + kStringsFolder + "/" + language + ".txt");
}

// Source modules' tables (docs/MODULES.md): modules/<name>/assets/strings/<language>.txt, in module name order
std::vector<std::string> moduleTablePaths(const std::string& language) {
    std::vector<std::string> out;
    const std::string modules = findAsset("modules");
    if (modules.empty()) return out;
    for (const std::string& m : listDirectories(modules)) {
        const std::string path = modules + "/" + m + "/assets/" + kStringsFolder + "/" + language + ".txt";
        if (fileExists(path)) out.push_back(path);
    }
    return out;
}

bool moduleTablesChanged(const Table& t) {
    const std::vector<std::string> paths = moduleTablePaths(t.language);
    if (paths.size() != t.modules.size()) return true;
    for (size_t i = 0; i < paths.size(); ++i)
        if (paths[i] != t.modules[i].first || fileWriteTime(paths[i]) != t.modules[i].second) return true;
    return false;
}

void readInto(const std::string& path, std::vector<StringEntry>& entries) {
    std::vector<StringEntry> read;
    std::string errors;
    readStringsFile(path, read, &errors);
    if (!errors.empty()) MEALKIT_WARN("Strings %s:\n%s", path.c_str(), errors.c_str());
    entries.insert(entries.end(), read.begin(), read.end());
}

// The project's table first, then each module's: on a key in both, the project's line wins
void loadTable(Table& t, const std::string& language) {
    t.language = language;
    t.path = tablePath(language);
    t.time = t.path.empty() ? 0 : fileWriteTime(t.path);
    t.entries.clear();
    t.modules.clear();
    if (!t.path.empty()) readInto(t.path, t.entries);
    for (const std::string& path : moduleTablePaths(language)) {
        t.modules.emplace_back(path, fileWriteTime(path));
        readInto(path, t.entries);
    }
    if (t.path.empty() && t.modules.empty()) return;
    std::stable_sort(t.entries.begin(), t.entries.end(),
                     [](const StringEntry& a, const StringEntry& b) { return a.key < b.key; });
    // If the same key appears twice, the earlier line is used
    t.entries.erase(std::unique(t.entries.begin(), t.entries.end(),
                                [](const StringEntry& a, const StringEntry& b) { return a.key == b.key; }),
                    t.entries.end());
}

} // namespace

bool validStringKey(std::string_view key) {
    if (key.empty()) return false;
    const char c0 = key[0];
    if (!((c0 >= 'a' && c0 <= 'z') || (c0 >= 'A' && c0 <= 'Z'))) return false;
    for (char c : key)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-'))
            return false;
    return true;
}

bool readStringsFile(const std::string& path, std::vector<StringEntry>& out, std::string* errors) {
    out.clear();
    std::string text;
    if (!readTextFile(path, text)) return false;
    if (text.size() >= 3 && static_cast<u8>(text[0]) == 0xEF && static_cast<u8>(text[1]) == 0xBB &&
        static_cast<u8>(text[2]) == 0xBF)
        text.erase(0, 3); // UTF-8 BOM
    u32 lineNo = 0;
    size_t pos = 0;
    while (pos <= text.size()) {
        size_t end = text.find('\n', pos);
        if (end == std::string::npos) end = text.size();
        const std::string line = trim(std::string_view(text).substr(pos, end - pos));
        ++lineNo;
        pos = end + 1;
        u32 version = 1; // only version 1 for now, so no migrate
        if (line.empty() || line[0] == '#' || text::readFormatLine(line, text::kStrings, path, version)) {
            if (end == text.size()) break;
            continue;
        }
        const size_t eq = line.find('=');
        const std::string key = eq == std::string::npos ? line : trim(std::string_view(line).substr(0, eq));
        if (eq == std::string::npos || !validStringKey(key)) {
            if (errors) *errors += "line " + std::to_string(lineNo) + ": expected \"key = text\" (key: letters, digits, . _ -)\n";
        } else {
            out.push_back({key, unescape(trim(std::string_view(line).substr(eq + 1))), lineNo});
        }
        if (end == text.size()) break;
    }
    return true;
}

struct Strings::Impl {
    Table current, fallback;
    f64 time = 0.0, nextCheck = 0.0;
    mutable std::deque<std::string> missing; // keys already warned about (returns the key name; a deque keeps references valid)

    void poll() {
        if (time < nextCheck) return;
        nextCheck = time + kFileCheckSeconds;
        for (Table* t : {&current, &fallback}) {
            if (t->language.empty()) continue;
            const std::string path = tablePath(t->language);
            const unsigned long long stamp = path.empty() ? 0 : fileWriteTime(path);
            if (path != t->path || stamp != t->time || moduleTablesChanged(*t)) {
                loadTable(*t, t->language);
                missing.clear();
                MEALKIT_INFO("Strings: %zu %s string(s)", t->entries.size(), t->language.c_str());
            }
        }
    }
};

Strings::Strings() : m(new Impl()) {}
Strings::~Strings() { delete m; }

void Strings::load(std::string language, std::string fallbackLanguage) {
    if (language.empty()) language = fallbackLanguage;
    loadTable(m->current, language);
    if (fallbackLanguage != language) loadTable(m->fallback, fallbackLanguage);
    else m->fallback = {};
    m->missing.clear();
    if (m->current.path.empty() && m->current.modules.empty())
        MEALKIT_WARN("Strings: assets/%s/%s.txt not found", kStringsFolder, language.c_str());
}

void Strings::update(f64 dt) {
    m->time += dt;
    m->poll();
}

const std::string& Strings::language() const { return m->current.language; }

std::vector<std::string> Strings::languages() const {
    std::vector<std::string> out;
    const std::string dir = findAsset(std::string("assets/") + kStringsFolder);
    if (dir.empty()) return out;
    for (const std::string& f : listFiles(dir, ".txt")) out.push_back(f.substr(0, f.size() - 4));
    return out;
}

bool Strings::has(std::string_view key) const { return m->current.find(key) || m->fallback.find(key); }

const std::string& Strings::get(std::string_view key) const {
    if (const std::string* s = m->current.find(key)) return *s;
    if (const std::string* s = m->fallback.find(key)) return *s;
    for (const std::string& k : m->missing)
        if (k == key) return k;
    m->missing.emplace_back(key);
    MEALKIT_WARN("Strings: no '%.*s' in assets/%s/%s.txt", static_cast<int>(key.size()), key.data(), kStringsFolder,
                 m->current.language.c_str());
    return m->missing.back();
}

std::string Strings::format(std::string_view key, std::initializer_list<std::pair<std::string_view, std::string>> args) const {
    const std::string& text = get(key);
    std::string out;
    out.reserve(text.size() + 16);
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '{') {
            const size_t close = text.find('}', i + 1);
            if (close != std::string::npos) {
                const std::string_view name = std::string_view(text).substr(i + 1, close - i - 1);
                bool replaced = false;
                for (const auto& [n, v] : args)
                    if (n == name) {
                        out += v;
                        replaced = true;
                        break;
                    }
                if (replaced) {
                    i = close;
                    continue;
                }
            }
        }
        out += text[i];
    }
    return out;
}

} // namespace mealkit::loc

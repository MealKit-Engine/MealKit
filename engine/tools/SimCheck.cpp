// SimCheck: determinism rule check for the game code's sim/ folder (build tool, run by mealkit_add_game before the build).
//
//   mealkit_simcheck <stamp file> [--third-party <folder>]... [--modules <folder>] <sim/ sources...>
//   --third-party: include folder of the project's third_party/<lib>/. Including headers in it from sim/ is SIM011
//   --modules: the project's modules/ folder. sim/ files of modules under it get SIM013 for including game source/,
//              and SIM014 for including another module not in module.txt's requires (docs/MODULES.md)
//
// Float literals (except 0, 1, 2, 0.5) are warning SIM012 (the build continues): game values belong in tuning/scene data. For algorithm constants, "// const-ok: reason" at the end of the line.
// Defaults of field declarations like MEALKIT_FIELD are data themselves, so they aren't checked.
// Checks token by token excluding comments, strings, and preprocessor lines (except #include). On a hit, reports what to use instead in MSVC format
// "file(line,col): error SIMnnn: ..." and fails (VS Code's $msCompile picks it up).
// If there are no problems, writes the stamp file so the next build rechecks only when sources change.
// Rationale for the rules: docs/CODING_MODEL.md "How it's enforced", "Determinism rules".
// Output is written in English, like logs.

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

enum class Tok { Ident, Number, String, Punct };

struct Token {
    Tok kind;
    std::string text;
    int line;
    int col;
};

struct Include {
    std::string path;
    bool angle; // <...>
    int line;
    int col;
};

struct Lexed {
    std::vector<Token> tokens;
    std::vector<Include> includes;
    std::vector<Token> conditionIdents; // identifiers in #if/#ifdef/#ifndef/#elif lines
    std::vector<int> constOkLines; // lines with a "// const-ok:" comment
};

bool isIdentStart(unsigned char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' || c >= 0x80; }
bool isIdentChar(unsigned char c) { return isIdentStart(c) || (c >= '0' && c <= '9'); }
bool isDigit(unsigned char c) { return c >= '0' && c <= '9'; }

class Lexer {
public:
    explicit Lexer(std::string_view src) : m_src(src) {
        if (m_src.substr(0, 3) == "\xEF\xBB\xBF") m_pos = 3;
    }

    Lexed run() {
        Lexed out;
        bool lineStart = true;
        while (m_pos < m_src.size()) {
            const char c = m_src[m_pos];
            if (c == '\n') {
                advance();
                lineStart = true;
                continue;
            }
            if (c == ' ' || c == '\t' || c == '\r' || c == '\f' || c == '\v') {
                advance();
                continue;
            }
            if (c == '\\' && peek(1) == '\n') { // line continuation
                advance();
                advance();
                continue;
            }
            if (c == '/' && peek(1) == '/') {
                const int commentLine = m_line;
                const size_t start = m_pos;
                skipLineComment();
                if (m_src.substr(start, m_pos - start).find("const-ok:") != std::string_view::npos)
                    out.constOkLines.push_back(commentLine);
                continue;
            }
            if (c == '/' && peek(1) == '*') {
                skipBlockComment();
                continue;
            }
            if (c == '#' && lineStart) {
                directive(out);
                lineStart = true;
                continue;
            }
            lineStart = false;
            const int line = m_line, col = m_col;
            if (isDigit(static_cast<unsigned char>(c)) || (c == '.' && isDigit(static_cast<unsigned char>(peek(1))))) {
                out.tokens.push_back({Tok::Number, number(), line, col});
            } else if (isIdentStart(static_cast<unsigned char>(c))) {
                std::string id = ident();
                const char next = m_pos < m_src.size() ? m_src[m_pos] : '\0';
                if (next == '"' && !id.empty() && id.back() == 'R' &&
                    (id == "R" || id == "u8R" || id == "uR" || id == "UR" || id == "LR")) {
                    rawString();
                    out.tokens.push_back({Tok::String, "\"\"", line, col});
                } else if ((next == '"' || next == '\'') && (id == "u8" || id == "u" || id == "U" || id == "L")) {
                    quoted(next);
                    out.tokens.push_back({Tok::String, "\"\"", line, col});
                } else {
                    out.tokens.push_back({Tok::Ident, std::move(id), line, col});
                }
            } else if (c == '"' || c == '\'') {
                quoted(c);
                out.tokens.push_back({Tok::String, "\"\"", line, col});
            } else if (c == ':' && peek(1) == ':') {
                advance();
                advance();
                out.tokens.push_back({Tok::Punct, "::", line, col});
            } else if (c == '-' && peek(1) == '>') {
                advance();
                advance();
                out.tokens.push_back({Tok::Punct, "->", line, col});
            } else {
                advance();
                out.tokens.push_back({Tok::Punct, std::string(1, c), line, col});
            }
        }
        return out;
    }

private:
    char peek(size_t ahead) const { return m_pos + ahead < m_src.size() ? m_src[m_pos + ahead] : '\0'; }

    void advance() {
        if (m_src[m_pos] == '\n') {
            ++m_line;
            m_col = 1;
        } else {
            ++m_col;
        }
        ++m_pos;
    }

    void skipLineComment() {
        while (m_pos < m_src.size() && m_src[m_pos] != '\n') {
            if (m_src[m_pos] == '\\' && peek(1) == '\n') advance(); // continued comment
            advance();
        }
    }

    void skipBlockComment() {
        advance();
        advance();
        while (m_pos < m_src.size() && !(m_src[m_pos] == '*' && peek(1) == '/')) advance();
        if (m_pos < m_src.size()) {
            advance();
            advance();
        }
    }

    std::string ident() {
        const size_t start = m_pos;
        while (m_pos < m_src.size() && isIdentChar(static_cast<unsigned char>(m_src[m_pos]))) advance();
        return std::string(m_src.substr(start, m_pos - start));
    }

    // pp-number: 1'000, 0x1Fu, 1.5e-3f
    std::string number() {
        const size_t start = m_pos;
        while (m_pos < m_src.size()) {
            const char c = m_src[m_pos];
            const char prev = m_pos > start ? m_src[m_pos - 1] : '\0';
            if (isIdentChar(static_cast<unsigned char>(c)) || c == '.') {
                advance();
            } else if (c == '\'' && isIdentChar(static_cast<unsigned char>(peek(1)))) {
                advance();
            } else if ((c == '+' || c == '-') && (prev == 'e' || prev == 'E' || prev == 'p' || prev == 'P')) {
                advance();
            } else {
                break;
            }
        }
        return std::string(m_src.substr(start, m_pos - start));
    }

    void quoted(char quote) {
        advance();
        while (m_pos < m_src.size() && m_src[m_pos] != quote && m_src[m_pos] != '\n') {
            if (m_src[m_pos] == '\\' && m_pos + 1 < m_src.size()) advance();
            advance();
        }
        if (m_pos < m_src.size() && m_src[m_pos] == quote) advance();
    }

    // R"delim( ... )delim"
    void rawString() {
        advance(); // "
        std::string delim;
        while (m_pos < m_src.size() && m_src[m_pos] != '(' && m_src[m_pos] != '\n') {
            delim += m_src[m_pos];
            advance();
        }
        const std::string end = ")" + delim + "\"";
        const size_t found = m_src.find(end, m_pos);
        const size_t stop = found == std::string_view::npos ? m_src.size() : found + end.size();
        while (m_pos < stop) advance();
    }

    // Preprocessor lines: only #include paths are recorded, the rest skipped
    void directive(Lexed& out) {
        const int line = m_line, col = m_col;
        std::string text;
        advance(); // #
        while (m_pos < m_src.size() && m_src[m_pos] != '\n') {
            if (m_src[m_pos] == '\\' && peek(1) == '\n') {
                advance();
                advance();
                continue;
            }
            if (m_src[m_pos] == '/' && peek(1) == '/') {
                skipLineComment();
                break;
            }
            if (m_src[m_pos] == '/' && peek(1) == '*') {
                skipBlockComment();
                text += ' ';
                continue;
            }
            if (m_src[m_pos] == '"' || m_src[m_pos] == '\'') { // strings containing "//" or "/*"
                const size_t start = m_pos;
                quoted(m_src[m_pos]);
                text += m_src.substr(start, m_pos - start);
                continue;
            }
            text += m_src[m_pos];
            advance();
        }
        size_t i = text.find_first_not_of(" \t");
        if (i == std::string::npos) return;
        if (text.compare(i, 2, "if") == 0 || text.compare(i, 4, "elif") == 0) {
            size_t k = i;
            while (k < text.size() && isIdentChar(static_cast<unsigned char>(text[k]))) ++k; // directive name
            while (k < text.size()) {
                if (isIdentStart(static_cast<unsigned char>(text[k]))) {
                    const size_t start = k;
                    while (k < text.size() && isIdentChar(static_cast<unsigned char>(text[k]))) ++k;
                    out.conditionIdents.push_back({Tok::Ident, text.substr(start, k - start), line, col});
                } else {
                    ++k;
                }
            }
            return;
        }
        if (text.compare(i, 7, "include") != 0) return;
        i = text.find_first_not_of(" \t", i + 7);
        if (i == std::string::npos) return;
        const char open = text[i];
        const char close = open == '<' ? '>' : open == '"' ? '"' : '\0';
        if (!close) return;
        const size_t j = text.find(close, i + 1);
        if (j == std::string::npos) return;
        out.includes.push_back({text.substr(i + 1, j - i - 1), open == '<', line, col});
    }

    std::string_view m_src;
    size_t m_pos = 0;
    int m_line = 1;
    int m_col = 1;
};

// ---------------------------------------------------------------- Rule messages

constexpr const char* kMsgMath =
    "CRT math is not bit-identical across CPUs and compilers. Use mealkit::detSin/detCos/detAtan2/detQuatFromAxisAngle "
    "(mealkit/core/DetMath.h), or compute the angle in view/ and send it as quantized input";
constexpr const char* kMsgRandom = "use mealkit::Pcg32 seeded from the match seed (core/Random.h)";
constexpr const char* kMsgTime = "real time differs between peers. Use the tick number (world.state.tick) and "
                                 "mealkit::sim::TickTimer (mealkit/sim/TickTimer.h)";
constexpr const char* kMsgAlloc =
    "no heap objects. Use mealkit::sim::Pool<T> (mealkit/sim/Pool.h) for objects created during a match, "
    "or fixed arrays sized in World::reset()";
constexpr const char* kMsgVirtual = "no inheritance or virtual Update. Use flat arrays with a kind enum and free "
                                    "system functions called from World::step()";
constexpr const char* kMsgAsync = "completion order differs between peers and cannot be hashed. Use "
                                  "mealkit::sim::TickTimer and enum class state machines";
constexpr const char* kMsgUnordered = "iteration order is not deterministic. Look up by id or use a sorted array";
constexpr const char* kMsgGlobal =
    "global mutable state breaks the shadow simulation and hot reload. Keep state in World, or make it constexpr";
constexpr const char* kMsgViewHeader =
    "the simulation must not read rendering, platform, input, editor or view state. Read World only; report to the "
    "screen with tick events (mealkit::sim::TickEvents)";
constexpr const char* kMsgFloatLiteral =
    "game values go in Tuning (game.tuning), scene objects or ViewTuning (docs/CODING_MODEL.md \"Where do numbers go\"). "
    "If it is an algorithm constant, end the line with '// const-ok: <reason>'";
constexpr const char* kMsgThirdParty =
    "game libraries are for view/ and app/ only. Simulation code uses the engine's verified tools (docs/CODING_MODEL.md "
    "\"Where external libraries go\")";
constexpr const char* kMsgUsingStd = "write std:: explicitly so the check can see what is used";
constexpr const char* kMsgModuleGame =
    "module code does not know the game. Take the module's own state as arguments and return results; the game calls "
    "the module from World::step() (docs/MODULES.md)";
constexpr const char* kMsgPlatformMacro =
    "the simulation must run the same code on every platform for crossplay. Move platform differences to view/ or app/ "
    "(docs/CODING_MODEL.md \"Determinism rules\")";
constexpr const char* kMsgModuleRequires = "add the module to 'requires' in this module's module.txt";

enum class Qual {
    Any,       // anywhere (except member access .x / ->x)
    StdOrNone, // unqualified or std:: / ::
    StdOnly,   // std:: only
};

struct IdentRule {
    const char* name;
    Qual qual;
    bool needsCall; // matches only when followed directly by (
    const char* code;
    const char* message;
};

constexpr IdentRule kIdentRules[] = {
    // CRT math (sqrt is allowed since IEEE 754 guarantees an exact result)
    {"sin", Qual::StdOrNone, true, "SIM001", kMsgMath},
    {"cos", Qual::StdOrNone, true, "SIM001", kMsgMath},
    {"tan", Qual::StdOrNone, true, "SIM001", kMsgMath},
    {"asin", Qual::StdOrNone, true, "SIM001", kMsgMath},
    {"acos", Qual::StdOrNone, true, "SIM001", kMsgMath},
    {"atan", Qual::StdOrNone, true, "SIM001", kMsgMath},
    {"atan2", Qual::StdOrNone, true, "SIM001", kMsgMath},
    {"sinh", Qual::StdOrNone, true, "SIM001", kMsgMath},
    {"cosh", Qual::StdOrNone, true, "SIM001", kMsgMath},
    {"tanh", Qual::StdOrNone, true, "SIM001", kMsgMath},
    {"exp", Qual::StdOrNone, true, "SIM001", kMsgMath},
    {"exp2", Qual::StdOrNone, true, "SIM001", kMsgMath},
    {"expm1", Qual::StdOrNone, true, "SIM001", kMsgMath},
    {"log", Qual::StdOrNone, true, "SIM001", kMsgMath},
    {"log2", Qual::StdOrNone, true, "SIM001", kMsgMath},
    {"log10", Qual::StdOrNone, true, "SIM001", kMsgMath},
    {"log1p", Qual::StdOrNone, true, "SIM001", kMsgMath},
    {"pow", Qual::StdOrNone, true, "SIM001", kMsgMath},
    {"cbrt", Qual::StdOrNone, true, "SIM001", kMsgMath},
    {"hypot", Qual::StdOrNone, true, "SIM001", kMsgMath},
    {"erf", Qual::StdOrNone, true, "SIM001", kMsgMath},
    {"tgamma", Qual::StdOrNone, true, "SIM001", kMsgMath},
    {"sinf", Qual::StdOrNone, true, "SIM001", kMsgMath},
    {"cosf", Qual::StdOrNone, true, "SIM001", kMsgMath},
    {"tanf", Qual::StdOrNone, true, "SIM001", kMsgMath},
    {"atanf", Qual::StdOrNone, true, "SIM001", kMsgMath},
    {"atan2f", Qual::StdOrNone, true, "SIM001", kMsgMath},
    {"expf", Qual::StdOrNone, true, "SIM001", kMsgMath},
    {"logf", Qual::StdOrNone, true, "SIM001", kMsgMath},
    {"powf", Qual::StdOrNone, true, "SIM001", kMsgMath},
    // Engine math functions that use CRT trig (core/Math.h)
    {"quatFromAxisAngle", Qual::Any, false, "SIM001", kMsgMath},
    {"perspectiveReversedZ", Qual::Any, false, "SIM001", kMsgMath},

    {"rand", Qual::StdOrNone, true, "SIM002", kMsgRandom},
    {"srand", Qual::StdOrNone, true, "SIM002", kMsgRandom},
    {"random_device", Qual::Any, false, "SIM002", kMsgRandom},
    {"mt19937", Qual::Any, false, "SIM002", kMsgRandom},
    {"mt19937_64", Qual::Any, false, "SIM002", kMsgRandom},
    {"minstd_rand", Qual::Any, false, "SIM002", kMsgRandom},
    {"default_random_engine", Qual::Any, false, "SIM002", kMsgRandom},
    {"uniform_int_distribution", Qual::Any, false, "SIM002", kMsgRandom},
    {"uniform_real_distribution", Qual::Any, false, "SIM002", kMsgRandom},
    {"normal_distribution", Qual::Any, false, "SIM002", kMsgRandom},

    {"time", Qual::StdOrNone, true, "SIM003", kMsgTime},
    {"clock", Qual::StdOrNone, true, "SIM003", kMsgTime},
    {"chrono", Qual::Any, false, "SIM003", kMsgTime},
    {"steady_clock", Qual::Any, false, "SIM003", kMsgTime},
    {"system_clock", Qual::Any, false, "SIM003", kMsgTime},
    {"high_resolution_clock", Qual::Any, false, "SIM003", kMsgTime},
    {"nowSeconds", Qual::Any, false, "SIM003", kMsgTime},
    {"QueryPerformanceCounter", Qual::Any, false, "SIM003", kMsgTime},
    {"GetTickCount", Qual::Any, false, "SIM003", kMsgTime},
    {"GetTickCount64", Qual::Any, false, "SIM003", kMsgTime},
    {"timeGetTime", Qual::Any, false, "SIM003", kMsgTime},

    {"make_unique", Qual::Any, false, "SIM004", kMsgAlloc},
    {"make_shared", Qual::Any, false, "SIM004", kMsgAlloc},
    {"unique_ptr", Qual::Any, false, "SIM004", kMsgAlloc},
    {"shared_ptr", Qual::Any, false, "SIM004", kMsgAlloc},
    {"malloc", Qual::StdOrNone, true, "SIM004", kMsgAlloc},
    {"calloc", Qual::StdOrNone, true, "SIM004", kMsgAlloc},
    {"realloc", Qual::StdOrNone, true, "SIM004", kMsgAlloc},
    {"free", Qual::StdOrNone, true, "SIM004", kMsgAlloc},

    {"virtual", Qual::Any, false, "SIM005", kMsgVirtual},
    {"override", Qual::Any, false, "SIM005", kMsgVirtual},

    {"co_await", Qual::Any, false, "SIM006", kMsgAsync},
    {"co_yield", Qual::Any, false, "SIM006", kMsgAsync},
    {"co_return", Qual::Any, false, "SIM006", kMsgAsync},
    {"thread", Qual::StdOnly, false, "SIM006", kMsgAsync},
    {"jthread", Qual::StdOnly, false, "SIM006", kMsgAsync},
    {"async", Qual::StdOnly, false, "SIM006", kMsgAsync},
    {"future", Qual::StdOnly, false, "SIM006", kMsgAsync},
    {"promise", Qual::StdOnly, false, "SIM006", kMsgAsync},
    {"packaged_task", Qual::StdOnly, false, "SIM006", kMsgAsync},
    {"mutex", Qual::StdOrNone, false, "SIM006", kMsgAsync},
    {"recursive_mutex", Qual::StdOrNone, false, "SIM006", kMsgAsync},
    {"shared_mutex", Qual::StdOrNone, false, "SIM006", kMsgAsync},
    {"atomic", Qual::StdOrNone, false, "SIM006", kMsgAsync},
    {"condition_variable", Qual::StdOrNone, false, "SIM006", kMsgAsync},

    {"unordered_map", Qual::Any, false, "SIM007", kMsgUnordered},
    {"unordered_set", Qual::Any, false, "SIM007", kMsgUnordered},
    {"unordered_multimap", Qual::Any, false, "SIM007", kMsgUnordered},
    {"unordered_multiset", Qual::Any, false, "SIM007", kMsgUnordered},

    {"thread_local", Qual::Any, false, "SIM008", kMsgGlobal},
};

// Platform/compiler/CPU macros: a branch on these in sim/ makes peers on different platforms run different code
constexpr const char* kPlatformMacros[] = {
    "_WIN32",       "_WIN64",      "__linux__",  "__APPLE__",   "__MACH__",   "__ANDROID__", "__EMSCRIPTEN__",
    "_MSC_VER",     "__clang__",   "__GNUC__",   "_M_X64",      "_M_IX86",    "_M_ARM64",    "__x86_64__",
    "__i386__",     "__aarch64__", "__arm__",    "__wasm__",    "__SSE__",    "__SSE2__",    "__SSE4_2__",
    "__AVX__",      "__AVX2__",    "__FMA__",    "__ARM_NEON",  "__ARM_FEATURE_FMA", "__unix__", "_XBOX_ONE",
    "_GAMING_XBOX", "__ORBIS__",   "__PROSPERO__", "__NX__",    "__SWITCH__",
};

enum class Match {
    Exact,   // the whole path matches (case-insensitive)
    Prefix,  // the path starts with this
    Segment, // this folder appears somewhere in the path ("view/", "../view/", "game/view/")
};

struct HeaderRule {
    const char* name;
    Match match;
    const char* code;
    const char* message;
};

constexpr HeaderRule kHeaderRules[] = {
    {"chrono", Match::Exact, "SIM003", kMsgTime},
    {"ctime", Match::Exact, "SIM003", kMsgTime},
    {"time.h", Match::Exact, "SIM003", kMsgTime},
    {"mealkit/core/Clock.h", Match::Exact, "SIM003", kMsgTime},
    {"random", Match::Exact, "SIM002", kMsgRandom},
    {"thread", Match::Exact, "SIM006", kMsgAsync},
    {"future", Match::Exact, "SIM006", kMsgAsync},
    {"mutex", Match::Exact, "SIM006", kMsgAsync},
    {"shared_mutex", Match::Exact, "SIM006", kMsgAsync},
    {"condition_variable", Match::Exact, "SIM006", kMsgAsync},
    {"atomic", Match::Exact, "SIM006", kMsgAsync},
    {"coroutine", Match::Exact, "SIM006", kMsgAsync},
    {"unordered_map", Match::Exact, "SIM007", kMsgUnordered},
    {"unordered_set", Match::Exact, "SIM007", kMsgUnordered},
    {"windows.h", Match::Exact, "SIM009", kMsgViewHeader},
    {"d3d12", Match::Prefix, "SIM009", kMsgViewHeader},
    {"dxgi", Match::Prefix, "SIM009", kMsgViewHeader},
    {"imgui", Match::Prefix, "SIM009", kMsgViewHeader},
    {"mealkit/render/", Match::Prefix, "SIM009", kMsgViewHeader},
    {"mealkit/platform/", Match::Prefix, "SIM009", kMsgViewHeader},
    {"mealkit/editor/", Match::Prefix, "SIM009", kMsgViewHeader},
    {"mealkit/game/", Match::Prefix, "SIM009", kMsgViewHeader},
    {"mealkit/ui/", Match::Prefix, "SIM009", kMsgViewHeader},
    {"mealkit/audio/", Match::Prefix, "SIM009", kMsgViewHeader},
    {"mealkit/settings/", Match::Prefix, "SIM009", kMsgViewHeader},
    {"mealkit/input/", Match::Prefix, "SIM009", kMsgViewHeader},
    {"mealkit/loc/", Match::Prefix, "SIM009", kMsgViewHeader},
    {"mealkit/feedback/", Match::Prefix, "SIM009", kMsgViewHeader},
    {"view/", Match::Segment, "SIM009", kMsgViewHeader},
    {"app/", Match::Segment, "SIM009", kMsgViewHeader},
};

char lower(char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; }

bool startsWithNoCase(std::string_view s, std::string_view prefix) {
    if (s.size() < prefix.size()) return false;
    for (size_t i = 0; i < prefix.size(); ++i)
        if (lower(s[i]) != lower(prefix[i])) return false;
    return true;
}

// Relative paths like "../view/Camera.h", "./sim/../app/x.h" are caught by folder name too
std::string normalizeInclude(const std::string& path) {
    std::string p = path;
    for (char& c : p)
        if (c == '\\') c = '/';
    for (;;) {
        if (p.rfind("../", 0) == 0) p.erase(0, 3);
        else if (p.rfind("./", 0) == 0) p.erase(0, 2);
        else break;
    }
    return p;
}

// ---------------------------------------------------------------- Checks

// Source modules (project modules/<name>/, docs/MODULES.md): which module a file is in and what it may include
struct ModuleInfo {
    std::filesystem::path modulesDir;
    std::string name;
    std::vector<std::string> requires_; // module.txt "requires name [minVersion] ..."

    bool allows(const std::string& other) const {
        return other == name || std::find(requires_.begin(), requires_.end(), other) != requires_.end();
    }
};

class Checker {
public:
    Checker(std::string path, const std::vector<std::string>& thirdParty, const ModuleInfo* module)
        : m_path(std::move(path)), m_thirdParty(thirdParty), m_module(module) {}

    int run(const Lexed& lexed) {
        for (const Include& inc : lexed.includes) checkInclude(inc);
        for (const Token& id : lexed.conditionIdents)
            for (const char* macro : kPlatformMacros)
                if (id.text == macro) report(id.line, id.col, "SIM015", "#if on " + id.text, kMsgPlatformMacro);
        m_t = &lexed.tokens;
        checkTokens();
        checkFloatLiterals(lexed.constOkLines);
        checkScopes();
        flush();
        return static_cast<int>(m_reports.size());
    }

private:
    void report(int line, int col, const char* code, const std::string& what, const char* message) {
        char buf[1024];
        std::snprintf(buf, sizeof(buf), "%s(%d,%d): error %s: %s is not allowed in sim/: %s\n", m_path.c_str(), line,
                      col, code, what.c_str(), message);
        m_reports.push_back({line, col, buf});
    }

    void warn(int line, int col, const char* code, const std::string& what, const char* message) {
        char buf[1024];
        std::snprintf(buf, sizeof(buf), "%s(%d,%d): warning %s: %s in sim/: %s\n", m_path.c_str(), line, col, code,
                      what.c_str(), message);
        m_warnings.push_back({line, col, buf});
    }

    // Float literals that aren't trivial values like 0/1/2/0.5 (excluding those inside field declaration macros and "// const-ok:" lines)
    void checkFloatLiterals(const std::vector<int>& constOkLines) {
        const std::vector<Token>& t = *m_t;
        int skipDepth = 0; // inside MEALKIT_* ( ... )
        for (size_t i = 0; i < t.size(); ++i) {
            if (skipDepth == 0 && t[i].kind == Tok::Ident && t[i].text.rfind("MEALKIT_", 0) == 0 && i + 1 < t.size() &&
                t[i + 1].text == "(") {
                skipDepth = 1;
                ++i;
                continue;
            }
            if (skipDepth > 0) {
                if (t[i].text == "(") ++skipDepth;
                else if (t[i].text == ")") --skipDepth;
                continue;
            }
            if (t[i].kind != Tok::Number) continue;
            const std::string& n = t[i].text;
            const bool hex = n.size() > 1 && n[0] == '0' && (n[1] == 'x' || n[1] == 'X');
            if (hex || (n.find('.') == std::string::npos && n.find_first_of("eE") == std::string::npos)) continue;
            std::string digits;
            for (char c : n)
                if (c != '\'' && c != 'f' && c != 'F' && c != 'l' && c != 'L') digits += c;
            const double v = std::strtod(digits.c_str(), nullptr);
            if (v == 0.0 || v == 1.0 || v == 2.0 || v == 0.5) continue;
            if (std::find(constOkLines.begin(), constOkLines.end(), t[i].line) != constOkLines.end()) continue;
            warn(t[i].line, t[i].col, "SIM012", "the float literal " + n, kMsgFloatLiteral);
        }
    }

    void flush() {
        std::stable_sort(m_reports.begin(), m_reports.end(), [](const Report& a, const Report& b) {
            return a.line != b.line ? a.line < b.line : a.col < b.col;
        });
        for (const Report& r : m_reports) std::fputs(r.text.c_str(), stdout);
        for (const Report& r : m_warnings) std::fputs(r.text.c_str(), stdout);
    }

    const Token* at(size_t i) const { return i < m_t->size() ? &(*m_t)[i] : nullptr; }
    bool is(size_t i, std::string_view text) const {
        const Token* t = at(i);
        return t && t->text == text && t->kind != Tok::String;
    }

    void checkInclude(const Include& inc) {
        const std::string p = normalizeInclude(inc.path);
        for (const HeaderRule& r : kHeaderRules) {
            bool match = false;
            switch (r.match) {
            case Match::Exact: match = p.size() == std::strlen(r.name) && startsWithNoCase(p, r.name); break;
            case Match::Prefix: match = startsWithNoCase(p, r.name); break;
            case Match::Segment:
                match = startsWithNoCase(p, r.name) || p.find(std::string("/") + r.name) != std::string::npos;
                break;
            }
            if (match) {
                report(inc.line, inc.col, r.code,
                       inc.angle ? "#include <" + inc.path + ">" : "#include \"" + inc.path + "\"", r.message);
                return;
            }
        }
        // Game libraries (project third_party/<lib>/): headers found in those folders, or paths containing third_party/
        std::error_code ec;
        bool thirdParty = startsWithNoCase(p, "third_party/") || p.find("/third_party/") != std::string::npos;
        for (const std::string& dir : m_thirdParty)
            thirdParty = thirdParty || std::filesystem::is_regular_file(std::filesystem::path(dir) / p, ec);
        if (thirdParty) {
            report(inc.line, inc.col, "SIM011",
                   inc.angle ? "#include <" + inc.path + ">" : "#include \"" + inc.path + "\"", kMsgThirdParty);
            return;
        }
        if (m_module) checkModuleInclude(inc, p);
    }

    // Module sim/ files: include only engine headers, standard headers, their own module, and modules in requires
    void checkModuleInclude(const Include& inc, const std::string& p) {
        namespace fs = std::filesystem;
        const std::string what = inc.angle ? "#include <" + inc.path + ">" : "#include \"" + inc.path + "\"";
        std::error_code ec;
        if (!inc.angle) {
            const fs::path near = fs::path(m_path).parent_path() / inc.path;
            if (fs::is_regular_file(near, ec)) {
                const fs::path rel = fs::weakly_canonical(near, ec).lexically_relative(m_module->modulesDir);
                const std::string first = rel.empty() ? std::string() : rel.begin()->string();
                if (first.empty() || first == ".." || !m_module->allows(first))
                    report(inc.line, inc.col, first.empty() || first == ".." ? "SIM013" : "SIM014", what,
                           first.empty() || first == ".." ? kMsgModuleGame : kMsgModuleRequires);
                return;
            }
        }
        const std::string first = p.substr(0, p.find('/'));
        if (first.size() < p.size() && fs::is_regular_file(m_module->modulesDir / first / "module.txt", ec)) {
            if (!m_module->allows(first)) report(inc.line, inc.col, "SIM014", what, kMsgModuleRequires);
            return;
        }
        if (startsWithNoCase(p, "mealkit/")) return;
        // A quoted include not found within the module is a header in game source/ ("sim/World.h"). Same for <sim/...>
        if (!inc.angle || startsWithNoCase(p, "sim/")) report(inc.line, inc.col, "SIM013", what, kMsgModuleGame);
    }

    // Qualifier: if directly preceded by . / -> it's member access; if ::, the name before it is the qualifier
    enum class Prefix { None, Member, Std, Global, Other };
    Prefix prefixOf(size_t i) const {
        if (i == 0) return Prefix::None;
        if (is(i - 1, ".") || is(i - 1, "->")) return Prefix::Member;
        if (!is(i - 1, "::")) return Prefix::None;
        if (i < 2) return Prefix::Global;
        const Token& q = (*m_t)[i - 2];
        if (q.kind == Tok::Ident) return q.text == "std" ? Prefix::Std : Prefix::Other;
        return Prefix::Global;
    }

    void checkTokens() {
        const std::vector<Token>& t = *m_t;
        for (size_t i = 0; i < t.size(); ++i) {
            if (t[i].kind != Tok::Ident) continue;
            const std::string& id = t[i].text;

            if (id == "new" && !is(i - 1, "operator")) {
                report(t[i].line, t[i].col, "SIM004", "'new'", kMsgAlloc);
                continue;
            }
            if (id == "delete" && i > 0 && !is(i - 1, "=") && !is(i - 1, "operator")) {
                report(t[i].line, t[i].col, "SIM004", "'delete'", kMsgAlloc);
                continue;
            }
            if (id == "using" && is(i + 1, "namespace") && is(i + 2, "std")) {
                report(t[i].line, t[i].col, "SIM010", "'using namespace std'", kMsgUsingStd);
                continue;
            }
            if (id == "static") {
                checkStatic(i);
                continue;
            }
            for (const IdentRule& r : kIdentRules) {
                if (id != r.name) continue;
                const Prefix p = prefixOf(i);
                bool hit = false;
                switch (r.qual) {
                case Qual::Any: hit = p != Prefix::Member; break;
                case Qual::StdOrNone: hit = p == Prefix::None || p == Prefix::Std || p == Prefix::Global; break;
                case Qual::StdOnly: hit = p == Prefix::Std; break;
                }
                if (hit && r.needsCall && !is(i + 1, "(")) hit = false;
                if (hit) report(t[i].line, t[i].col, r.code, "'" + id + "'", r.message);
                break;
            }
        }
    }

    // static variables are allowed only if const/constexpr. If ( comes first, it's a function declaration.
    void checkStatic(size_t i) {
        const std::vector<Token>& t = *m_t;
        for (size_t j = i + 1; j < t.size(); ++j) {
            const std::string& s = t[j].text;
            if (t[j].kind == Tok::Ident && (s == "const" || s == "constexpr")) return;
            if (t[j].kind != Tok::Punct) continue;
            if (s == "(") return;
            if (s == ";" || s == "=" || s == "{" || s == "[" || s == ",") {
                report(t[i].line, t[i].col, "SIM008", "a mutable static variable", kMsgGlobal);
                return;
            }
        }
    }

    // Mutable global variables at namespace scope (declared without static)
    enum class Scope { Namespace, Class, FunctionBody, Initializer };

    void checkScopes() {
        const std::vector<Token>& t = *m_t;
        std::vector<Scope> scopes{Scope::Namespace};
        size_t stmtStart = 0;
        bool stmtChecked = false;

        for (size_t i = 0; i < t.size(); ++i) {
            if (t[i].kind != Tok::Punct) continue;
            const std::string& s = t[i].text;
            const bool atNamespace = scopes.back() == Scope::Namespace;
            if (s == "{") {
                Scope kind = Scope::FunctionBody;
                if (atNamespace) {
                    kind = classifyBrace(stmtStart, i);
                    if (kind == Scope::Initializer && !stmtChecked) {
                        checkGlobalDecl(stmtStart, i);
                        stmtChecked = true;
                    }
                    if (kind == Scope::Namespace) {
                        stmtStart = i + 1;
                        stmtChecked = false;
                    }
                } else if (scopes.back() == Scope::Class) {
                    kind = Scope::FunctionBody; // member function bodies, nested types, and member initializers are all out of scope
                }
                scopes.push_back(kind);
            } else if (s == "}") {
                const Scope closed = scopes.back();
                if (scopes.size() > 1) scopes.pop_back();
                if (scopes.back() == Scope::Namespace) {
                    // T g = { ... }; and struct X { ... }; are one statement up to ; (already judged).
                    // Anything else (the body after a constructor's m_x{1} initializer etc.) counts as the statement ending.
                    const bool continues = (closed == Scope::Initializer || closed == Scope::Class) &&
                                           (is(i + 1, ";") || is(i + 1, ",") || closed == Scope::Class);
                    if (continues) {
                        stmtChecked = true;
                    } else {
                        stmtStart = i + 1;
                        stmtChecked = false;
                    }
                }
            } else if (s == ";" && atNamespace) {
                if (!stmtChecked) checkGlobalDecl(stmtStart, i);
                stmtStart = i + 1;
                stmtChecked = false;
            }
        }
    }

    Scope classifyBrace(size_t begin, size_t end) const {
        const std::vector<Token>& t = *m_t;
        bool paren = false, assign = false;
        for (size_t j = begin; j < end; ++j) {
            const std::string& s = t[j].text;
            if (t[j].kind == Tok::Ident && s == "namespace") return Scope::Namespace;
            if (t[j].kind == Tok::Ident && s == "extern" && j + 1 < end && t[j + 1].kind == Tok::String)
                return Scope::Namespace;
            if (t[j].kind == Tok::Ident && (s == "class" || s == "struct" || s == "union" || s == "enum") && !paren &&
                !assign)
                return Scope::Class;
            if (t[j].kind == Tok::Punct && s == "(") paren = true;
            if (t[j].kind == Tok::Punct && s == "=") assign = true;
        }
        if (paren && !assign) return Scope::FunctionBody;
        return Scope::Initializer;
    }

    void checkGlobalDecl(size_t begin, size_t end) {
        const std::vector<Token>& t = *m_t;
        if (begin >= end) return;
        const std::string& first = t[begin].text;
        if (t[begin].kind != Tok::Ident) return; // [[attr]] etc. are ignored
        if (first == "using" || first == "namespace" || first == "template" || first == "typedef" ||
            first == "static_assert" || first == "class" || first == "struct" || first == "union" || first == "enum" ||
            first == "friend" || first == "concept")
            return;
        size_t limit = end;
        for (size_t j = begin; j < end; ++j) {
            if (t[j].kind == Tok::Punct && t[j].text == "=") {
                limit = j; // ( inside an initializer expression isn't a function declaration
                break;
            }
        }
        for (size_t j = begin; j < limit; ++j) {
            const std::string& s = t[j].text;
            if (t[j].kind == Tok::Punct && s == "(") return; // function declaration
            if (t[j].kind == Tok::Ident && (s == "const" || s == "constexpr" || s == "static" || s == "operator"))
                return;
        }
        if (limit == begin) return;
        report(t[begin].line, t[begin].col, "SIM008", "a mutable global variable", kMsgGlobal);
    }

    struct Report {
        int line;
        int col;
        std::string text;
    };

    std::string m_path;
    const std::vector<std::string>& m_thirdParty;
    const ModuleInfo* m_module = nullptr; // nullptr if not a module file
    const std::vector<Token>* m_t = nullptr;
    std::vector<Report> m_reports;
    std::vector<Report> m_warnings; // don't stop the build
};

bool readFile(const char* path, std::string& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}

// If the file is under modulesDir/<name>/, reads requires from that module's module.txt
bool findModule(const std::filesystem::path& modulesDir, const char* file, ModuleInfo& out) {
    namespace fs = std::filesystem;
    if (modulesDir.empty()) return false;
    std::error_code ec;
    const fs::path rel = fs::weakly_canonical(fs::path(file), ec).lexically_relative(modulesDir);
    if (rel.empty() || rel.begin()->string() == "..") return false;
    out.modulesDir = modulesDir;
    out.name = rel.begin()->string();
    out.requires_.clear();
    std::string text;
    if (!readFile((modulesDir / out.name / "module.txt").string().c_str(), text)) return true;
    std::istringstream lines(text);
    for (std::string line; std::getline(lines, line);) {
        std::istringstream words(line);
        std::string key;
        if (!(words >> key) || key != "requires") continue;
        for (std::string w; words >> w;)
            if (!(w[0] >= '0' && w[0] <= '9')) out.requires_.push_back(w); // starting with a digit means the previous module's minimum version
    }
    return true;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::printf("usage: mealkit_simcheck <stamp-file> [--third-party <dir>]... <sim sources...>\n");
        return 2;
    }
    std::remove(argv[1]); // on failure the stamp must not exist so the next build rechecks
    std::vector<std::string> thirdParty;
    int first = 2;
    while (first + 1 < argc && std::string(argv[first]) == "--third-party") {
        thirdParty.push_back(argv[first + 1]);
        first += 2;
    }
    std::filesystem::path modulesDir;
    if (first + 1 < argc && std::string(argv[first]) == "--modules") {
        std::error_code ec;
        modulesDir = std::filesystem::weakly_canonical(argv[first + 1], ec);
        first += 2;
    }
    int errors = 0;
    for (int i = first; i < argc; ++i) {
        std::string src;
        if (!readFile(argv[i], src)) {
            std::printf("%s: error SIM000: cannot read file\n", argv[i]);
            ++errors;
            continue;
        }
        ModuleInfo module;
        const bool inModule = findModule(modulesDir, argv[i], module);
        Checker checker(argv[i], thirdParty, inModule ? &module : nullptr);
        errors += checker.run(Lexer(src).run());
    }
    if (errors) {
        std::printf("SimCheck: %d problem(s) in sim/ code. Rules: docs/CODING_MODEL.md in the engine folder "
                    "(\"Determinism rules\" section).\n",
                    errors);
        std::fflush(stdout);
        return 1;
    }
    std::ofstream stamp(argv[1], std::ios::binary | std::ios::trunc);
    stamp << "ok\n";
    return stamp ? 0 : 1;
}

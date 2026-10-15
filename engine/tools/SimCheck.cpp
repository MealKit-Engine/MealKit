// SimCheck: 게임 코드 sim/ 폴더의 결정론 규칙 검사 (빌드 도구, mealkit_add_game이 빌드 전에 실행한다).
//
//   mealkit_simcheck <stamp 파일> <sim/ 소스...>
//
// 주석, 문자열, 전처리 줄(#include 제외)은 빼고 토큰 단위로 검사한다. 걸리면 MSVC 형식
// "파일(줄,열): error SIMnnn: ..."으로 대신 쓸 것을 알려 주고 실패한다(VS Code $msCompile이 잡는다).
// 문제가 없으면 stamp 파일을 써서 다음 빌드에서는 소스가 바뀔 때만 다시 검사한다.
// 규칙의 근거: docs/CODING_MODEL.md "강제하는 방법", README "결정론 규칙".
// 출력은 로그와 같이 영어로 쓴다.

#include <algorithm>
#include <cstdio>
#include <cstring>
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
            if (c == '\\' && peek(1) == '\n') { // 줄 이어 쓰기
                advance();
                advance();
                continue;
            }
            if (c == '/' && peek(1) == '/') {
                skipLineComment();
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
                if (next == '"' && !id.empty() && id.back() == 'R' && (id == "R" || id == "u8R" || id == "uR" || id == "UR" || id == "LR")) {
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
            if (m_src[m_pos] == '\\' && peek(1) == '\n') advance(); // 이어지는 주석
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

    // 전처리 줄: #include 경로만 기록하고 나머지는 건너뛴다
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
            if (m_src[m_pos] == '"' || m_src[m_pos] == '\'') { // "//" 나 "/*" 가 든 문자열
                const size_t start = m_pos;
                quoted(m_src[m_pos]);
                text += m_src.substr(start, m_pos - start);
                continue;
            }
            text += m_src[m_pos];
            advance();
        }
        size_t i = text.find_first_not_of(" \t");
        if (i == std::string::npos || text.compare(i, 7, "include") != 0) return;
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

// ---------------------------------------------------------------- 규칙 메시지

constexpr const char* kMsgMath =
    "CRT math is not bit-identical across CPUs and compilers. Use +, -, *, / and std::sqrt only, "
    "or compute the angle in view/ and send it as quantized input";
constexpr const char* kMsgRandom = "use mealkit::Pcg32 seeded from the match seed (core/Random.h)";
constexpr const char* kMsgTime =
    "real time differs between peers. Use the tick number (world.state.tick) and mealkit::sim::TickTimer (mealkit/sim/TickTimer.h)";
constexpr const char* kMsgAlloc =
    "no heap objects. Use mealkit::sim::Pool<T> (mealkit/sim/Pool.h) for objects created during a match, "
    "or fixed arrays sized in World::reset()";
constexpr const char* kMsgVirtual =
    "no inheritance or virtual Update. Use flat arrays with a kind enum and free system functions called from World::step()";
constexpr const char* kMsgAsync =
    "completion order differs between peers and cannot be hashed. Use mealkit::sim::TickTimer and enum class state machines";
constexpr const char* kMsgUnordered = "iteration order is not deterministic. Look up by id or use a sorted array";
constexpr const char* kMsgGlobal =
    "global mutable state breaks the shadow simulation and hot reload. Keep state in World, or make it constexpr";
constexpr const char* kMsgViewHeader =
    "the simulation must not read rendering, platform, input, editor or view state. Read World only; report to the screen with tick events (mealkit::sim::TickEvents)";
constexpr const char* kMsgUsingStd = "write std:: explicitly so the check can see what is used";

enum class Qual {
    Any,       // 어디서든 (멤버 접근 .x / ->x 은 제외)
    StdOrNone, // 한정 없음 또는 std:: / ::
    StdOnly,   // std:: 만
};

struct IdentRule {
    const char* name;
    Qual qual;
    bool needsCall; // 바로 뒤에 ( 가 와야 걸린다
    const char* code;
    const char* message;
};

constexpr IdentRule kIdentRules[] = {
    // CRT 수학 (sqrt는 IEEE 754가 정확한 결과를 보장하므로 허용)
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
    // 엔진 수학 중 CRT 삼각함수를 쓰는 것 (core/Math.h)
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

enum class Match {
    Exact,   // 경로 전체가 같다 (대소문자 무시)
    Prefix,  // 경로가 이것으로 시작한다
    Segment, // 경로 어딘가에 이 폴더가 있다 ("view/", "../view/", "game/view/")
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

// "../view/Camera.h", "./sim/../app/x.h" 같은 상대 경로도 폴더 이름으로 잡는다
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

// ---------------------------------------------------------------- 검사

class Checker {
public:
    explicit Checker(std::string path) : m_path(std::move(path)) {}

    int run(const Lexed& lexed) {
        for (const Include& inc : lexed.includes) checkInclude(inc);
        m_t = &lexed.tokens;
        checkTokens();
        checkScopes();
        flush();
        return static_cast<int>(m_reports.size());
    }

private:
    void report(int line, int col, const char* code, const std::string& what, const char* message) {
        char buf[1024];
        std::snprintf(buf, sizeof(buf), "%s(%d,%d): error %s: %s is not allowed in sim/: %s\n", m_path.c_str(), line, col, code,
                      what.c_str(), message);
        m_reports.push_back({line, col, buf});
    }

    void flush() {
        std::stable_sort(m_reports.begin(), m_reports.end(),
                         [](const Report& a, const Report& b) { return a.line != b.line ? a.line < b.line : a.col < b.col; });
        for (const Report& r : m_reports) std::fputs(r.text.c_str(), stdout);
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
                report(inc.line, inc.col, r.code, inc.angle ? "#include <" + inc.path + ">" : "#include \"" + inc.path + "\"", r.message);
                return;
            }
        }
    }

    // 한정자: 바로 앞이 . / -> 이면 멤버 접근, :: 이면 그 앞 이름이 한정자
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

    // static 변수는 const/constexpr만 허용. ( 가 먼저 나오면 함수 선언이다.
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

    // 네임스페이스 범위의 가변 전역 변수 (static 없이 선언한 것)
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
                    kind = Scope::FunctionBody; // 멤버 함수 본문, 중첩 타입, 멤버 초기화 모두 검사 대상이 아니다
                }
                scopes.push_back(kind);
            } else if (s == "}") {
                const Scope closed = scopes.back();
                if (scopes.size() > 1) scopes.pop_back();
                if (scopes.back() == Scope::Namespace) {
                    // T g = { ... }; 와 struct X { ... }; 는 ; 까지 한 문장이다 (이미 판정했다).
                    // 그 밖(생성자의 m_x{1} 초기화 뒤 본문 등)은 문장이 끝난 것으로 본다.
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
            if (t[j].kind == Tok::Ident && s == "extern" && j + 1 < end && t[j + 1].kind == Tok::String) return Scope::Namespace;
            if (t[j].kind == Tok::Ident && (s == "class" || s == "struct" || s == "union" || s == "enum") && !paren && !assign)
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
        if (t[begin].kind != Tok::Ident) return; // [[attr]] 등은 무시
        if (first == "using" || first == "namespace" || first == "template" || first == "typedef" || first == "static_assert" ||
            first == "class" || first == "struct" || first == "union" || first == "enum" || first == "friend" || first == "concept")
            return;
        size_t limit = end;
        for (size_t j = begin; j < end; ++j) {
            if (t[j].kind == Tok::Punct && t[j].text == "=") {
                limit = j; // 초기화 식 안의 ( 는 함수 선언이 아니다
                break;
            }
        }
        for (size_t j = begin; j < limit; ++j) {
            const std::string& s = t[j].text;
            if (t[j].kind == Tok::Punct && s == "(") return; // 함수 선언
            if (t[j].kind == Tok::Ident && (s == "const" || s == "constexpr" || s == "static" || s == "operator")) return;
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
    const std::vector<Token>* m_t = nullptr;
    std::vector<Report> m_reports;
};

bool readFile(const char* path, std::string& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::printf("usage: mealkit_simcheck <stamp-file> <sim sources...>\n");
        return 2;
    }
    std::remove(argv[1]); // 실패하면 stamp가 없어야 다음 빌드에서 다시 검사한다
    int errors = 0;
    for (int i = 2; i < argc; ++i) {
        std::string src;
        if (!readFile(argv[i], src)) {
            std::printf("%s: error SIM000: cannot read file\n", argv[i]);
            ++errors;
            continue;
        }
        Checker checker(argv[i]);
        errors += checker.run(Lexer(src).run());
    }
    if (errors) {
        std::printf("SimCheck: %d problem(s) in sim/ code. Rules: docs/CODING_MODEL.md (engine), README \"Determinism rules\".\n", errors);
        std::fflush(stdout);
        return 1;
    }
    std::ofstream stamp(argv[1], std::ios::binary | std::ios::trunc);
    stamp << "ok\n";
    return stamp ? 0 : 1;
}

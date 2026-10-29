#pragma once

#include <cstdio>
#include <vector>

#include <string>

// A tiny test registry. Per test file: TEST_CASE(area, name) { ... TEST_CHECK(cond); }
// area is an engine folder name (core, net, render ...). --only area,area runs only those areas (tools\check.ps1 -Changed).
namespace mealkit::test {

struct Case {
    const char* area;
    const char* name;
    void (*fn)();
};

inline std::vector<Case>& cases() {
    static std::vector<Case> list;
    return list;
}

inline int& failures() {
    static int count = 0;
    return count;
}

struct Register {
    Register(const char* area, const char* name, void (*fn)()) { cases().push_back({area, name, fn}); }
};

inline void fail(const char* cond, const char* file, int line) {
    std::printf("  FAIL %s (%s:%d)\n", cond, file, line);
    ++failures();
}

// only: comma-separated list of areas, all if empty
inline bool selected(const char* area, const std::string& only) {
    if (only.empty()) return true;
    const std::string list = "," + only + ",";
    return list.find("," + std::string(area) + ",") != std::string::npos;
}

inline int runAll(const std::string& only = {}) {
    int ran = 0;
    for (const Case& c : cases()) {
        if (!selected(c.area, only)) continue;
        ++ran;
        const int before = failures();
        c.fn();
        std::printf("%s %s\n", failures() == before ? "ok  " : "FAIL", c.name);
    }
    if (!only.empty()) std::printf("ran %d of %d test(s) (--only %s)\n", ran, static_cast<int>(cases().size()), only.c_str());
    std::printf(failures() ? "FAILED (%d)\n" : "PASS\n", failures());
    std::fflush(stdout);
    return failures() ? 1 : 0;
}

} // namespace mealkit::test

#define TEST_CASE(area, name)                                                                                          \
    void test_##name();                                                                                                \
    const ::mealkit::test::Register register_##name(#area, #name, &test_##name);                                       \
    void test_##name()

#define TEST_CHECK(cond)                                                                                               \
    do {                                                                                                               \
        if (!(cond)) ::mealkit::test::fail(#cond, __FILE__, __LINE__);                                                 \
    } while (0)

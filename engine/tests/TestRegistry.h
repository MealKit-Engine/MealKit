#pragma once

#include <cstdio>
#include <vector>

// 아주 작은 테스트 등록기. 테스트 파일마다 TEST_CASE(name) { ... TEST_CHECK(cond); }
namespace mealkit::test {

struct Case {
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
    Register(const char* name, void (*fn)()) { cases().push_back({name, fn}); }
};

inline void fail(const char* cond, const char* file, int line) {
    std::printf("  FAIL %s (%s:%d)\n", cond, file, line);
    ++failures();
}

inline int runAll() {
    for (const Case& c : cases()) {
        const int before = failures();
        c.fn();
        std::printf("%s %s\n", failures() == before ? "ok  " : "FAIL", c.name);
    }
    std::printf(failures() ? "FAILED (%d)\n" : "PASS\n", failures());
    std::fflush(stdout);
    return failures() ? 1 : 0;
}

} // namespace mealkit::test

#define TEST_CASE(name)                                                    \
    void test_##name();                                                    \
    const ::mealkit::test::Register register_##name(#name, &test_##name); \
    void test_##name()

#define TEST_CHECK(cond)                                                       \
    do {                                                                       \
        if (!(cond)) ::mealkit::test::fail(#cond, __FILE__, __LINE__);         \
    } while (0)

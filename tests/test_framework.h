#pragma once

// Harness mínimo de testes (~80 linhas, zero dependências).
// Uso: TEST(suite, nome) { EXPECT_TRUE(...); } — main roda tudo ou filtra por argv[1].

#include <cstdio>
#include <cstring>
#include <cmath>

namespace tmtest {

struct TestCase {
    const char* suite;
    const char* name;
    void (*fn)();
};

inline int& Count() { static int n = 0; return n; }
inline TestCase* Registry() { static TestCase tests[256]; return tests; }
inline int& Failures() { static int f = 0; return f; }
inline const char*& CurrentTest() { static const char* t = ""; return t; }

inline void Register(const char* suite, const char* name, void (*fn)()) {
    Registry()[Count()] = {suite, name, fn};
    Count()++;
}

inline void Fail(const char* expr, const char* file, int line) {
    Failures()++;
    printf("  FAIL [%s] %s:%d: %s\n", CurrentTest(), file, line, expr);
}

inline int RunAll(const char* filter) {
    int ran = 0, failedBefore = Failures();
    for (int i = 0; i < Count(); i++) {
        if (filter && strcmp(filter, Registry()[i].suite) != 0)
            continue;
        CurrentTest() = Registry()[i].name;
        printf("[RUN ] %s.%s\n", Registry()[i].suite, Registry()[i].name);
        Registry()[i].fn();
        ran++;
    }
    int failures = Failures() - failedBefore;
    printf("%s: %d testes, %d falhas\n", failures ? "FAIL" : "OK", ran, failures);
    return failures ? 1 : 0;
}

}

#define TEST(suite, name) \
    static void suite##_##name(); \
    static bool suite##_##name##_reg = (tmtest::Register(#suite, #name, suite##_##name), true); \
    static void suite##_##name()

#define EXPECT_TRUE(x)  do { if (!(x)) tmtest::Fail(#x, __FILE__, __LINE__); } while (0)
#define EXPECT_FALSE(x) do { if ((x)) tmtest::Fail("!" #x, __FILE__, __LINE__); } while (0)
#define EXPECT_EQ(a, b) do { if ((a) != (b)) tmtest::Fail(#a " == " #b, __FILE__, __LINE__); } while (0)
#define EXPECT_LE(a, b) do { if (!((a) <= (b))) tmtest::Fail(#a " <= " #b, __FILE__, __LINE__); } while (0)
#define EXPECT_NEAR(a, b, eps) do { if (fabsf((a) - (b)) > (eps)) tmtest::Fail(#a " ~= " #b, __FILE__, __LINE__); } while (0)

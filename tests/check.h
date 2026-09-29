#pragma once

// assert() compiles to nothing under NDEBUG, and both CI and the release
// verification run ctest against a Release build -- so assert-based tests
// silently pass no matter what. CHECK always evaluates its condition and
// always fails the process.

#include <cstdio>
#include <cstdlib>

namespace offwork::testing {

inline void Fail(const char* expression, const char* file, int line) {
    std::fprintf(stderr, "CHECK failed: %s\n  at %s:%d\n", expression, file, line);
    std::fflush(stderr);
    std::abort();
}

inline void Check(bool condition, const char* expression, const char* file, int line) {
    if (!condition) {
        Fail(expression, file, line);
    }
}

}  // namespace offwork::testing

#define CHECK(condition) \
    ::offwork::testing::Check((condition), #condition, __FILE__, __LINE__)

// TestFramework.h - a tiny zero-dependency test harness.
//
// Hand-rolled rather than GoogleTest because the whole point is that these tests
// run with nothing installed but a compiler. Adding a dependency to test an
// Arduino project would undercut the "just open it and go" goal.
#ifndef SMARTSOCKET_TEST_FRAMEWORK_H
#define SMARTSOCKET_TEST_FRAMEWORK_H

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace testing {

struct TestCase {
  const char* name;
  void (*fn)();
};

inline std::vector<TestCase>& registry() {
  static std::vector<TestCase> tests;
  return tests;
}

inline int& failureCount() {
  static int count = 0;
  return count;
}

inline bool& currentFailed() {
  static bool failed = false;
  return failed;
}

struct Registrar {
  Registrar(const char* name, void (*fn)()) {
    registry().push_back(TestCase{name, fn});
  }
};

inline void reportFailure(const char* file, int line, const std::string& what) {
  currentFailed() = true;
  failureCount()++;
  std::printf("    FAIL %s:%d\n      %s\n", file, line, what.c_str());
}

inline int runAll() {
  int passed = 0;
  int failed = 0;

  for (size_t i = 0; i < registry().size(); ++i) {
    currentFailed() = false;
    std::printf("  %s\n", registry()[i].name);
    registry()[i].fn();
    if (currentFailed()) {
      ++failed;
    } else {
      ++passed;
    }
  }

  std::printf("\n=====================================\n");
  std::printf("  %d passed, %d failed, %d total\n", passed, failed,
              passed + failed);
  std::printf("=====================================\n");
  return failed == 0 ? 0 : 1;
}

}  // namespace testing

#define TEST(name)                                                      \
  static void name();                                                   \
  static testing::Registrar registrar_##name(#name, name);              \
  static void name()

#define CHECK(cond)                                                     \
  do {                                                                  \
    if (!(cond)) {                                                      \
      testing::reportFailure(__FILE__, __LINE__,                        \
                             std::string("expected true: ") + #cond);   \
    }                                                                   \
  } while (0)

#define CHECK_FALSE(cond)                                               \
  do {                                                                  \
    if ((cond)) {                                                       \
      testing::reportFailure(__FILE__, __LINE__,                        \
                             std::string("expected false: ") + #cond);  \
    }                                                                   \
  } while (0)

#define CHECK_EQ(actual, expected)                                      \
  do {                                                                  \
    const long long a_ = static_cast<long long>(actual);                \
    const long long e_ = static_cast<long long>(expected);              \
    if (a_ != e_) {                                                     \
      char buf_[256];                                                   \
      std::snprintf(buf_, sizeof(buf_),                                 \
                    "%s == %s\n        actual:   %lld"                  \
                    "\n        expected: %lld",                         \
                    #actual, #expected, a_, e_);                        \
      testing::reportFailure(__FILE__, __LINE__, buf_);                 \
    }                                                                   \
  } while (0)

#define CHECK_STR_EQ(actual, expected)                                  \
  do {                                                                  \
    const char* a_ = (actual);                                          \
    const char* e_ = (expected);                                        \
    if (std::strcmp(a_, e_) != 0) {                                     \
      char buf_[256];                                                   \
      std::snprintf(buf_, sizeof(buf_),                                 \
                    "%s\n        actual:   [%s]"                        \
                    "\n        expected: [%s]",                         \
                    #actual, a_, e_);                                   \
      testing::reportFailure(__FILE__, __LINE__, buf_);                 \
    }                                                                   \
  } while (0)

#define CHECK_NEAR(actual, expected, tolerance)                         \
  do {                                                                  \
    const long long a_ = static_cast<long long>(actual);                \
    const long long e_ = static_cast<long long>(expected);              \
    const long long t_ = static_cast<long long>(tolerance);             \
    const long long d_ = a_ > e_ ? a_ - e_ : e_ - a_;                   \
    if (d_ > t_) {                                                      \
      char buf_[256];                                                   \
      std::snprintf(buf_, sizeof(buf_),                                 \
                    "%s near %s\n        actual:   %lld"                \
                    "\n        expected: %lld (+/- %lld)",              \
                    #actual, #expected, a_, e_, t_);                    \
      testing::reportFailure(__FILE__, __LINE__, buf_);                 \
    }                                                                   \
  } while (0)

#endif  // SMARTSOCKET_TEST_FRAMEWORK_H

// adtest.hpp - minimal dependency-free unit test harness.
// Chosen over gtest/doctest so the tests also build on OpenWrt/musl toolchains.
#ifndef __ADTEST_HPP_
#define __ADTEST_HPP_
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>

namespace adtest {
typedef void (*test_fn)();
struct test_entry {
  const char *name;
  test_fn fn;
};
inline std::vector<test_entry> &registry() {
  static std::vector<test_entry> r;
  return r;
}
inline int &failures() {
  static int f = 0;
  return f;
}
struct registrar {
  registrar(const char *name, test_fn fn) {
    test_entry e = {name, fn};
    registry().push_back(e);
  }
};
inline int run_all(int argc, char **argv) {
  int total_failed = 0;
  int ran = 0;
  for (size_t i = 0; i < registry().size(); i++) {
    const test_entry &t = registry()[i];
    if (argc > 1 && strstr(t.name, argv[1]) == NULL)
      continue;
    int before = failures();
    printf("[ RUN      ] %s\n", t.name);
    fflush(stdout);
    t.fn();
    ran++;
    if (failures() != before) {
      total_failed++;
      printf("[  FAILED  ] %s\n", t.name);
    } else
      printf("[       OK ] %s\n", t.name);
    fflush(stdout);
  }
  printf("%d test(s) run, %d failed\n", ran, total_failed);
  return total_failed == 0 ? 0 : 1;
}
} // namespace adtest

#define ADTEST_CAT2(a, b) a##b
#define ADTEST_CAT(a, b) ADTEST_CAT2(a, b)
#define TEST_CASE(name)                                                        \
  static void ADTEST_CAT(adtest_fn_, __LINE__)();                              \
  static adtest::registrar ADTEST_CAT(adtest_reg_, __LINE__)(                  \
      name, &ADTEST_CAT(adtest_fn_, __LINE__));                                \
  static void ADTEST_CAT(adtest_fn_, __LINE__)()
#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      printf("%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond);          \
      adtest::failures()++;                                                    \
    }                                                                          \
  } while (0)
#define REQUIRE(cond)                                                          \
  do {                                                                         \
    if (!(cond)) {                                                             \
      printf("%s:%d: REQUIRE failed: %s\n", __FILE__, __LINE__, #cond);        \
      adtest::failures()++;                                                    \
      return;                                                                  \
    }                                                                          \
  } while (0)
#define CHECK_EQ(a, b)                                                         \
  do {                                                                         \
    if (!((a) == (b))) {                                                       \
      printf("%s:%d: CHECK_EQ failed: %s == %s\n", __FILE__, __LINE__, #a,     \
             #b);                                                              \
      adtest::failures()++;                                                    \
    }                                                                          \
  } while (0)
#define ADTEST_MAIN()                                                          \
  int main(int argc, char **argv) { return adtest::run_all(argc, argv); }
#endif

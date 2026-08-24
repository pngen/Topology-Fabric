// tests/test_harness.hpp - minimal dependency-free test harness.
#pragma once
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <functional>
#include <sstream>

namespace tf_test {

struct TestCase {
  std::string name;
  std::function<void()> fn;
};

inline std::vector<TestCase>& registry() {
  static std::vector<TestCase> r;
  return r;
}

inline int& failures() { static int f = 0; return f; }

struct Registrar {
  Registrar(const char* name, std::function<void()> fn) {
    registry().push_back(TestCase{name, std::move(fn)});
  }
};

inline void fail(const char* file, int line, const std::string& msg) {
  ++failures();
  std::fprintf(stderr, "  FAIL %s:%d: %s\n", file, line, msg.c_str());
}

inline int run_all() {
  int ran = 0;
  for (auto& t : registry()) {
    std::fprintf(stderr, "[ RUN ] %s\n", t.name.c_str());
    int before = failures();
    try {
      t.fn();
    } catch (const std::exception& e) {
      ++failures();
      std::fprintf(stderr, "  FAIL (exception) %s\n", e.what());
    } catch (...) {
      ++failures();
      std::fprintf(stderr, "  FAIL (unknown exception)\n");
    }
    if (failures() == before) std::fprintf(stderr, "[  OK ] %s\n", t.name.c_str());
    else std::fprintf(stderr, "[FAIL ] %s\n", t.name.c_str());
    ++ran;
  }
  std::fprintf(stderr, "\n%zu test(s) ran, %d failure(s)\n", registry().size(), failures());
  return failures() == 0 ? 0 : 1;
}

}  // namespace tf_test

#define TF_TEST(name) \
  static void tf_test_##name(); \
  static ::tf_test::Registrar tf_reg_##name(#name, &tf_test_##name); \
  static void tf_test_##name()

#define ASSERT(cond) do { if (!(cond)) ::tf_test::fail(__FILE__, __LINE__, "assertion failed: " #cond); } while (0)
#define ASSERT_EQ(a, b) do { \
  auto _va = (a); auto _vb = (b); \
  if (!(_va == _vb)) { std::ostringstream _oss; _oss << #a << " == " << #b << " (got: '" << _va << "' vs '" << _vb << "')"; ::tf_test::fail(__FILE__, __LINE__, _oss.str()); } \
} while (0)
#define ASSERT_NEAR(a, b, eps) do { double _va=(a); double _vb=(b); if (!(_va >= _vb-(eps) && _va <= _vb+(eps))) { std::ostringstream _oss; _oss << #a << " ~= " << #b << " (got " << _va << " vs " << _vb << ")"; ::tf_test::fail(__FILE__, __LINE__, _oss.str()); } } while (0)
#define ASSERT_THROWS(stmt) do { bool _threw=false; try { stmt; } catch (...) { _threw=true; } if(!_threw) ::tf_test::fail(__FILE__, __LINE__, "expected exception from: " #stmt); } while (0)
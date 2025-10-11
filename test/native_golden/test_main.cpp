#include <unity.h>

#include "../../tests/native_golden/harness.h"
#include "../../tests/native_golden/harness.cpp"

using seedbox::tests::golden::render_fixture;
using seedbox::tests::golden::RenderOptions;

void test_golden_harness_stubs() {
#ifndef ENABLE_GOLDEN
  TEST_IGNORE_MESSAGE("ENABLE_GOLDEN disabled; skipping golden audio harness.");
#else
  RenderOptions options{.name = "sprout", .duration_seconds = 1.0f, .quiet_mode = true};
  auto report = render_fixture(options);
  TEST_ASSERT_GREATER_THAN_UINT32(0, report.samples);
  TEST_ASSERT_FALSE(report.wrote_fixture);
#endif
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_golden_harness_stubs);
  return UNITY_END();
}

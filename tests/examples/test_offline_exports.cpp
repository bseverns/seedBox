#include <unity.h>

#include <filesystem>
#include <string>

// Pull in the shared renderer implementation so the embedded examples link.
#include "../../examples/shared/offline_renderer.cpp"

// Rebrand the example entry points so we can invoke them directly.
#define main sprout_example_main
#include "../../examples/01_sprout/src/main.cpp"
#undef main

#define main headless_example_main
#include "../../examples/03_headless/src/main.cpp"
#undef main

namespace {

std::filesystem::path projectRoot() {
  return std::filesystem::current_path();
}

void removeIfExists(const std::filesystem::path& p) {
  std::error_code ec;
  std::filesystem::remove(p, ec);
}

}  // namespace

void setUp() {}
void tearDown() {}

void test_examples_emit_artifacts() {
  const auto root = projectRoot();
  const auto sproutWav = root / "out" / "intro-sprout.wav";
  const auto headlessWav = root / "out" / "headless-automation.wav";
  const auto headlessJson = root / "out" / "headless-automation.json";

  removeIfExists(sproutWav);
  removeIfExists(headlessWav);
  removeIfExists(headlessJson);

  char sproutProg[] = "sprout";
  char sproutQuiet[] = "--quiet";
  char sproutExport[] = "--export-wav";
  char* sproutArgs[] = {sproutProg, sproutQuiet, sproutExport};
  TEST_ASSERT_EQUAL_INT_MESSAGE(0, sprout_example_main(3, sproutArgs), "sprout example failed");
  TEST_ASSERT_TRUE_MESSAGE(std::filesystem::exists(sproutWav), "sprout WAV missing");
  TEST_ASSERT_TRUE_MESSAGE(std::filesystem::file_size(sproutWav) > 0, "sprout WAV empty");

  char headlessProg[] = "headless";
  char headlessExport[] = "--export";
  char* headlessArgs[] = {headlessProg, headlessExport};
  TEST_ASSERT_EQUAL_INT_MESSAGE(0, headless_example_main(2, headlessArgs), "headless example failed");
  TEST_ASSERT_TRUE_MESSAGE(std::filesystem::exists(headlessWav), "headless WAV missing");
  TEST_ASSERT_TRUE_MESSAGE(std::filesystem::file_size(headlessWav) > 0, "headless WAV empty");
  TEST_ASSERT_TRUE_MESSAGE(std::filesystem::exists(headlessJson), "headless JSON missing");
  TEST_ASSERT_TRUE_MESSAGE(std::filesystem::file_size(headlessJson) > 0, "headless JSON empty");
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_examples_emit_artifacts);
  return UNITY_END();
}

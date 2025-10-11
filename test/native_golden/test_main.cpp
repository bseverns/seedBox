#include <Arduino.h>
#include <unity.h>

#include "../../tests/native_golden/harness.h"

using namespace seedbox::tests::golden;

namespace {
constexpr std::size_t kChannels = 2;
constexpr std::size_t kFrames = 48000;  // 1 second @ 48 kHz
float gBuffer[kChannels * kFrames] = {0.0f};
}

void test_golden_render_stub(void) {
#if defined(ENABLE_GOLDEN) && ENABLE_GOLDEN
  RenderSpec spec{.frames = kFrames, .sampleRate = 48000};
  const RenderResult result = renderSproutFixture(gBuffer, kChannels, spec);
  TEST_ASSERT_TRUE_MESSAGE(result.rendered, result.note);
#else
  TEST_IGNORE_MESSAGE("Golden fixtures disabled; define ENABLE_GOLDEN to activate.");
#endif
}

void setUp(void) {}
void tearDown(void) {}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_golden_render_stub);
  return UNITY_END();
}

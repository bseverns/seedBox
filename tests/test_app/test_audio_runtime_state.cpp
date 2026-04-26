#include <unity.h>

#include <cmath>

#include "app/AudioRuntimeState.h"

void test_audio_runtime_state_tracks_flags_and_processes_audio() {
  AudioRuntimeState runtime;
  TEST_ASSERT_FALSE(runtime.hostAudioMode());
  TEST_ASSERT_FALSE(runtime.testToneEnabled());
  TEST_ASSERT_EQUAL_UINT64(0u, runtime.audioCallbackCount());

  runtime.resetHostState(true);
  runtime.setTestToneEnabled(true);
  runtime.incrementAudioCallbackCount();

  float left[8]{};
  float right[8]{};
  runtime.renderTestTone(left, right, 8u, 48000.0f);
  TEST_ASSERT_TRUE(runtime.hostAudioMode());
  TEST_ASSERT_TRUE(runtime.testToneEnabled());
  TEST_ASSERT_EQUAL_UINT64(1u, runtime.audioCallbackCount());
  bool foundLeft = false;
  bool foundRight = false;
  for (std::size_t i = 0; i < 8; ++i) {
    foundLeft = foundLeft || (std::fabs(left[i]) > 0.0f);
    foundRight = foundRight || (std::fabs(right[i]) > 0.0f);
  }
  TEST_ASSERT_TRUE(foundLeft);
  TEST_ASSERT_TRUE(foundRight);

  float hotLeft[4]{1.2f, 1.1f, 0.9f, 1.0f};
  float hotRight[4]{1.2f, 1.1f, 0.9f, 1.0f};
  runtime.applyHostOutputSafety(hotLeft, hotRight, 4u);
  for (float sample : hotLeft) {
    TEST_ASSERT_TRUE(std::fabs(sample) <= 0.9f);
  }
  for (float sample : hotRight) {
    TEST_ASSERT_TRUE(std::fabs(sample) <= 0.9f);
  }
}

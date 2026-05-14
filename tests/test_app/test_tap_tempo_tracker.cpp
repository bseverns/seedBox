#include <unity.h>

#include "app/TapTempoTracker.h"

void test_tap_tempo_tracker_builds_bpm_and_resets_pending_tap() {
  TapTempoTracker tracker;

  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 120.0f, tracker.currentBpm());
  TEST_ASSERT_EQUAL_UINT32(0u, tracker.lastIntervalMs());
  TEST_ASSERT_FALSE(tracker.noteTap(1000000).has_value());

  const auto interval = tracker.noteTap(1500000);
  TEST_ASSERT_TRUE(interval.has_value());
  TEST_ASSERT_EQUAL_UINT32(500u, *interval);
  TEST_ASSERT_EQUAL_UINT32(500u, tracker.lastIntervalMs());
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 120.0f, tracker.currentBpm());

  tracker.resetPendingTap();
  TEST_ASSERT_FALSE(tracker.noteTap(3000000).has_value());
  TEST_ASSERT_EQUAL_UINT32(500u, tracker.lastIntervalMs());
}

void test_tap_tempo_tracker_caps_history() {
  TapTempoTracker tracker;
  for (std::uint32_t i = 1; i <= 10; ++i) {
    tracker.recordInterval(100u * i);
  }

  TEST_ASSERT_EQUAL_UINT32(1000u, tracker.lastIntervalMs());
  const float expected = 60000.0f / 650.0f;
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, expected, tracker.currentBpm());
}

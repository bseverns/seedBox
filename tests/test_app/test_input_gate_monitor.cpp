#include <unity.h>

#include "app/InputGateMonitor.h"

void test_input_gate_monitor_tracks_dry_input_and_arms_once_per_hot_edge() {
  InputGateMonitor monitor;
  monitor.setFloor(0.05f);

  const float hotLeft[8]{0.08f, 0.08f, 0.08f, 0.08f, 0.08f, 0.08f, 0.08f, 0.08f};
  monitor.setDryInput(hotLeft, nullptr, 8u);

  TEST_ASSERT_TRUE(monitor.hasDryInput());
  TEST_ASSERT_TRUE(monitor.hot());
  TEST_ASSERT_TRUE(monitor.gateEdgePending());
  TEST_ASSERT_NOT_NULL(monitor.dryRight(8u));
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, monitor.dryLeft()[0], monitor.dryRight(8u)[0]);

  TEST_ASSERT_FALSE(monitor.gateReady(5u, 6u));
  TEST_ASSERT_TRUE(monitor.gateReady(6u, 6u));

  monitor.syncGateTick(6u);
  TEST_ASSERT_FALSE(monitor.gateEdgePending());

  monitor.refreshFromDryInput(8u);
  TEST_ASSERT_FALSE_MESSAGE(monitor.gateEdgePending(), "steady hot input should not retrigger until it goes cold");

  const float cold[8]{};
  monitor.setDryInput(cold, nullptr, 8u);
  TEST_ASSERT_FALSE(monitor.hot());

  const float hotRight[8]{0.10f, 0.10f, 0.10f, 0.10f, 0.10f, 0.10f, 0.10f, 0.10f};
  monitor.setDryInput(hotLeft, hotRight, 8u);
  TEST_ASSERT_TRUE(monitor.gateEdgePending());
  TEST_ASSERT_NOT_NULL(monitor.dryRight(8u));
  TEST_ASSERT_NOT_EQUAL(monitor.dryLeft(), monitor.dryRight(8u));
}

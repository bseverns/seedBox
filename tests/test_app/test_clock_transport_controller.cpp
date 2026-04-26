#include <unity.h>

#include "app/ClockTransportController.h"
#include "engine/Patterns.h"

void test_clock_transport_controller_toggles_provider_and_follow_flag() {
  InternalClock internal{};
  MidiClockIn midiIn{};
  MidiClockOut midiOut{};
  PatternScheduler scheduler{};
  ClockTransportController controller(internal, midiIn, midiOut, scheduler);

  TEST_ASSERT_EQUAL_PTR(&internal, controller.clock());
  TEST_ASSERT_FALSE(controller.followExternalClockEnabled());
  TEST_ASSERT_FALSE(controller.externalClockDominant());

  controller.toggleClockProvider();
  TEST_ASSERT_EQUAL_PTR(&midiIn, controller.clock());
  TEST_ASSERT_TRUE(controller.followExternalClockEnabled());
  TEST_ASSERT_TRUE(controller.externalClockDominant());

  controller.toggleClockProvider();
  TEST_ASSERT_EQUAL_PTR(&internal, controller.clock());
  TEST_ASSERT_FALSE(controller.followExternalClockEnabled());
  TEST_ASSERT_FALSE(controller.externalClockDominant());
}

void test_clock_transport_controller_latches_transport_gate() {
  InternalClock internal{};
  MidiClockIn midiIn{};
  MidiClockOut midiOut{};
  PatternScheduler scheduler{};
  ClockTransportController controller(internal, midiIn, midiOut, scheduler);

  controller.setTransportLatchEnabled(true);
  controller.handleTransportGate(127);
  TEST_ASSERT_TRUE(controller.transportLatchEnabled());
  TEST_ASSERT_TRUE(controller.externalTransportRunning());
  TEST_ASSERT_TRUE(controller.transportLatchedRunning());

  controller.handleTransportGate(0);
  TEST_ASSERT_TRUE(controller.externalTransportRunning());
  TEST_ASSERT_TRUE(controller.transportLatchedRunning());

  controller.handleTransportGate(127);
  TEST_ASSERT_FALSE(controller.externalTransportRunning());
  TEST_ASSERT_FALSE(controller.transportLatchedRunning());
}

void test_clock_transport_controller_watchdog_falls_back_to_internal() {
  InternalClock internal{};
  MidiClockIn midiIn{};
  MidiClockOut midiOut{};
  PatternScheduler scheduler{};
  ClockTransportController controller(internal, midiIn, midiOut, scheduler);

  controller.setFollowExternalClockEnabled(true, 1000u);
  TEST_ASSERT_TRUE(controller.followExternalClockEnabled());
  TEST_ASSERT_EQUAL_PTR(&midiIn, controller.clock());

  controller.updateExternalClockWatchdog(2500u, 2000u);
  TEST_ASSERT_TRUE(controller.followExternalClockEnabled());
  TEST_ASSERT_FALSE(controller.waitingForExternalClock());

  controller.updateExternalClockWatchdog(3101u, 2000u);
  TEST_ASSERT_FALSE(controller.followExternalClockEnabled());
  TEST_ASSERT_FALSE(controller.waitingForExternalClock());
  TEST_ASSERT_EQUAL_PTR(&internal, controller.clock());
  TEST_ASSERT_FALSE(controller.externalClockDominant());
}

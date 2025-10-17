#include <unity.h>
#include "app/AppState.h"
#include "interop/mn42_map.h"

using namespace seedbox::interop;

// MN42 handshake plus follow mode toggling. The script walks through hello →
// follow enable → external clock ownership → follow disable. Think of it as a
// narrated log of how the firmware should react when the controller boots.
void test_mn42_handshake_and_clock_follow() {
  AppState app;
  app.initSim();

  const uint64_t baseline = app.schedulerTicks();
  const uint8_t mn42Channel = static_cast<uint8_t>(mn42::kDefaultChannel + 1);
  app.onExternalControlChange(mn42Channel, mn42::cc::kHandshake, mn42::handshake::kHello);
  TEST_ASSERT_EQUAL_UINT8(mn42::handshake::kHello, app.mn42HandshakeValue());

  app.tick();
  TEST_ASSERT_TRUE(app.schedulerTicks() > baseline);

  app.onExternalControlChange(mn42Channel, mn42::cc::kMode, mn42::mode::kFollowExternalClock);
  TEST_ASSERT_EQUAL_UINT8(mn42::mode::kFollowExternalClock, app.mn42ModeBits());
  TEST_ASSERT_TRUE(app.followExternalClockEnabled());

  app.onExternalControlChange(mn42Channel, mn42::cc::kTransportGate, 127);
  TEST_ASSERT_TRUE(app.transportLatched());

  const uint64_t externalBaseline = app.schedulerTicks();
  app.tick();
  TEST_ASSERT_EQUAL_UINT64(externalBaseline, app.schedulerTicks());

  app.onExternalClockTick();
  TEST_ASSERT_TRUE(app.schedulerTicks() > externalBaseline);

  app.onExternalControlChange(mn42Channel, mn42::cc::kTransportGate, 0);
  TEST_ASSERT_FALSE(app.transportLatched());
  const uint64_t pausedBaseline = app.schedulerTicks();
  app.tick();
  TEST_ASSERT_EQUAL_UINT64(pausedBaseline, app.schedulerTicks());

  app.onExternalControlChange(mn42Channel, mn42::cc::kMode, 0);
  TEST_ASSERT_FALSE(app.followExternalClockEnabled());
  const uint64_t resumeBaseline = app.schedulerTicks();
  app.tick();
  TEST_ASSERT_TRUE(app.schedulerTicks() > resumeBaseline);
}

// Debug metering and transport latch toggles share the same CC bit field.
// This test double checks both the exposed flags and how the latch/momentary
// semantics flip when MN42 hammers CC 17.
void test_mn42_debug_and_transport_latch_modes() {
  AppState app;
  app.initSim();

  TEST_ASSERT_FALSE(app.debugMetersExposed());
  TEST_ASSERT_FALSE(app.transportLatchEnabled());

  const uint8_t mn42Channel = static_cast<uint8_t>(mn42::kDefaultChannel + 1);
  const uint8_t debugLatchMask = mn42::mode::kExposeDebugMeters | mn42::mode::kLatchTransport;
  app.onExternalControlChange(mn42Channel, mn42::cc::kMode, debugLatchMask);
  TEST_ASSERT_TRUE(app.debugMetersExposed());
  TEST_ASSERT_TRUE(app.transportLatchEnabled());
  TEST_ASSERT_FALSE(app.transportLatched());

  app.onExternalControlChange(mn42Channel, mn42::cc::kTransportGate, 100);
  TEST_ASSERT_TRUE(app.transportLatched());

  app.onExternalControlChange(mn42Channel, mn42::cc::kTransportGate, 100);
  TEST_ASSERT_FALSE(app.transportLatched());

  app.onExternalControlChange(mn42Channel, mn42::cc::kMode, mn42::mode::kExposeDebugMeters);
  TEST_ASSERT_TRUE(app.debugMetersExposed());
  TEST_ASSERT_FALSE(app.transportLatchEnabled());
  TEST_ASSERT_FALSE(app.transportLatched());

  app.onExternalControlChange(mn42Channel, mn42::cc::kTransportGate, 64);
  TEST_ASSERT_TRUE(app.transportLatched());

  app.onExternalControlChange(mn42Channel, mn42::cc::kTransportGate, 0);
  TEST_ASSERT_FALSE(app.transportLatched());
}

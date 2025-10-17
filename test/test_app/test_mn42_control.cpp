#include <unity.h>
#include "app/AppState.h"
#include "interop/mn42_map.h"

void test_mn42_follow_clock_mode() {
  AppState app;
  app.initSim();

  TEST_ASSERT_FALSE(app.followExternalClockEnabled());
  TEST_ASSERT_FALSE(app.externalClockDominant());

  const uint64_t baseline = app.schedulerTicks();
  app.tick();
  TEST_ASSERT_TRUE(app.schedulerTicks() > baseline);

  app.onExternalControlChange(seedbox::interop::mn42::kDefaultChannel,
                              seedbox::interop::mn42::cc::kHandshake,
                              seedbox::interop::mn42::handshake::kHello);
  TEST_ASSERT_TRUE(app.mn42HelloSeen());

  app.onExternalControlChange(seedbox::interop::mn42::kDefaultChannel,
                              seedbox::interop::mn42::cc::kMode,
                              seedbox::interop::mn42::mode::kFollowExternalClock);

  TEST_ASSERT_TRUE(app.followExternalClockEnabled());
  const uint64_t frozen = app.schedulerTicks();
  app.tick();
  TEST_ASSERT_EQUAL_UINT64(frozen, app.schedulerTicks());
  TEST_ASSERT_TRUE(app.externalClockDominant());

  app.onExternalControlChange(seedbox::interop::mn42::kDefaultChannel,
                              seedbox::interop::mn42::cc::kMode,
                              0);

  TEST_ASSERT_FALSE(app.followExternalClockEnabled());
  TEST_ASSERT_FALSE(app.externalClockDominant());
  app.tick();
  TEST_ASSERT_TRUE(app.schedulerTicks() > frozen);
}

void test_mn42_debug_meter_toggle() {
  AppState app;
  app.initSim();

  TEST_ASSERT_FALSE(app.debugMetersEnabled());
  app.onExternalControlChange(seedbox::interop::mn42::kDefaultChannel,
                              seedbox::interop::mn42::cc::kMode,
                              seedbox::interop::mn42::mode::kExposeDebugMeters);
  TEST_ASSERT_TRUE(app.debugMetersEnabled());

  app.onExternalControlChange(seedbox::interop::mn42::kDefaultChannel,
                              seedbox::interop::mn42::cc::kMode,
                              0);
  TEST_ASSERT_FALSE(app.debugMetersEnabled());
}

void test_mn42_transport_latch_behavior() {
  AppState app;
  app.initSim();

  app.onExternalControlChange(seedbox::interop::mn42::kDefaultChannel,
                              seedbox::interop::mn42::cc::kTransportGate,
                              100);
  TEST_ASSERT_TRUE(app.externalTransportRunning());
  TEST_ASSERT_TRUE(app.externalClockDominant());

  app.onExternalControlChange(seedbox::interop::mn42::kDefaultChannel,
                              seedbox::interop::mn42::cc::kTransportGate,
                              0);
  TEST_ASSERT_FALSE(app.externalTransportRunning());
  TEST_ASSERT_FALSE(app.externalClockDominant());

  app.onExternalControlChange(seedbox::interop::mn42::kDefaultChannel,
                              seedbox::interop::mn42::cc::kMode,
                              seedbox::interop::mn42::mode::kLatchTransport);
  TEST_ASSERT_TRUE(app.transportLatchEnabled());
  TEST_ASSERT_FALSE(app.externalTransportRunning());

  app.onExternalControlChange(seedbox::interop::mn42::kDefaultChannel,
                              seedbox::interop::mn42::cc::kTransportGate,
                              120);
  TEST_ASSERT_TRUE(app.externalTransportRunning());
  TEST_ASSERT_TRUE(app.transportLatchedRunning());

  app.onExternalControlChange(seedbox::interop::mn42::kDefaultChannel,
                              seedbox::interop::mn42::cc::kTransportGate,
                              0);
  TEST_ASSERT_TRUE(app.externalTransportRunning());
  TEST_ASSERT_TRUE(app.transportLatchedRunning());

  app.onExternalControlChange(seedbox::interop::mn42::kDefaultChannel,
                              seedbox::interop::mn42::cc::kTransportGate,
                              110);
  TEST_ASSERT_FALSE(app.externalTransportRunning());
  TEST_ASSERT_FALSE(app.transportLatchedRunning());

  app.onExternalControlChange(seedbox::interop::mn42::kDefaultChannel,
                              seedbox::interop::mn42::cc::kTransportGate,
                              0);
  TEST_ASSERT_FALSE(app.externalTransportRunning());
  TEST_ASSERT_FALSE(app.transportLatchedRunning());
}

void test_mn42_usb_channel_normalization() {
  AppState app;
  app.initSim();

  // Raw USB traffic reports channel 1 by default; confirm we ignore it until
  // the router converts it to zero-based form.
  app.onExternalControlChange(1, seedbox::interop::mn42::cc::kHandshake,
                              seedbox::interop::mn42::handshake::kHello);
  TEST_ASSERT_FALSE(app.mn42HelloSeen());

  const uint8_t normalized =
      seedbox::interop::mn42::NormalizeUsbChannel(1 /* usb-style channel */);
  TEST_ASSERT_EQUAL_UINT8(seedbox::interop::mn42::kDefaultChannel, normalized);

  app.onExternalControlChange(normalized, seedbox::interop::mn42::cc::kHandshake,
                              seedbox::interop::mn42::handshake::kHello);
  TEST_ASSERT_TRUE(app.mn42HelloSeen());

  app.onExternalControlChange(normalized, seedbox::interop::mn42::cc::kMode,
                              seedbox::interop::mn42::mode::kFollowExternalClock);
  TEST_ASSERT_TRUE(app.followExternalClockEnabled());
}

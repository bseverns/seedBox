// This suite mirrors the MN42 handshake/mode contract so we can spot drift the
// moment someone bumps the CC map.  For the full table (and bigger-picture
// narrative), hit docs/interop_mn42.md.  Headline constants: cc::kHandshake
// (14), cc::kMode (15), and cc::kTransportGate (17) should line up with the
// firmware's story.

#include <unity.h>
#include "app/AppState.h"
#include "interop/mn42_map.h"
#include "SeedBoxConfig.h"

#define private public
#include "io/MidiRouter.h"
#undef private

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

void test_mn42_hello_resends_ack() {
  MidiRouter router;
  router.begin();

  // App is ready, so the first HELLO should emit an ACK and set the flag.
  router.mn42AppReady_ = true;
  router.handleMn42ControlChange(seedbox::interop::mn42::kDefaultChannel,
                                 seedbox::interop::mn42::cc::kHandshake,
                                 seedbox::interop::mn42::handshake::kHello);
  TEST_ASSERT_TRUE(router.mn42AckSent_);

  // A new HELLO arriving before the app is ready must drop the sent flag so the
  // controller will hear the ACK once we're back online.
  router.mn42AppReady_ = false;
  router.handleMn42ControlChange(seedbox::interop::mn42::kDefaultChannel,
                                 seedbox::interop::mn42::cc::kHandshake,
                                 seedbox::interop::mn42::handshake::kHello);
  TEST_ASSERT_FALSE(router.mn42AckSent_);

  // Flip readiness back on and confirm we immediately fire a fresh ACK.
  router.mn42AppReady_ = true;
  router.handleMn42ControlChange(seedbox::interop::mn42::kDefaultChannel,
                                 seedbox::interop::mn42::cc::kHandshake,
                                 seedbox::interop::mn42::handshake::kHello);
  TEST_ASSERT_TRUE(router.mn42AckSent_);
}

void test_quiet_mode_follow_flag_survives_tick() {
  if (!SeedBoxConfig::kQuietMode) {
    TEST_IGNORE_MESSAGE("quiet-mode regression runs only when QUIET_MODE=1");
    return;
  }

  AppState app;
  app.initSim();

  // Prime the quiet-mode reset path so legacy behaviour (re-entering
  // primeSeeds() every frame) would have clobbered the follow flag.
  app.tick();

  TEST_ASSERT_FALSE(app.followExternalClockEnabled());

  app.onExternalControlChange(seedbox::interop::mn42::kDefaultChannel,
                              seedbox::interop::mn42::cc::kMode,
                              seedbox::interop::mn42::mode::kFollowExternalClock);

  TEST_ASSERT_TRUE(app.followExternalClockEnabled());

  app.tick();

  // Regression check: followExternalClockEnabled() should stay latched even
  // though quiet mode keeps the audio engines muted.
  TEST_ASSERT_TRUE(app.followExternalClockEnabled());
}

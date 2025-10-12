#include <unity.h>
#include <cstring>
#include "app/AppState.h"

namespace {
constexpr uint8_t kEngineCycleCc = 20;
}

// Exercise the MIDI CC engine-cycling path. The test doubles as documentation
// for how the UI encoder should behave: CC 20 with a value >= 64 rotates the
// focused seed forward, lower values rotate backward, and the OLED snapshot
// mirrors the change.
void test_cc_cycles_engine_and_snapshot_updates() {
  AppState app;
  app.initSim();

  TEST_ASSERT_FALSE(app.seeds().empty());

  AppState::DisplaySnapshot snap{};
  app.captureDisplaySnapshot(snap);
  TEST_ASSERT_NOT_NULL(strstr(snap.status, "SMP"));

  app.onExternalControlChange(0, kEngineCycleCc, 127);
  TEST_ASSERT_EQUAL_UINT8(1, app.seeds()[app.focusSeed()].engine);

  app.captureDisplaySnapshot(snap);
  TEST_ASSERT_NOT_NULL(strstr(snap.status, "GRA"));

  app.onExternalControlChange(0, kEngineCycleCc, 0);
  TEST_ASSERT_EQUAL_UINT8(0, app.seeds()[app.focusSeed()].engine);

  app.captureDisplaySnapshot(snap);
  TEST_ASSERT_NOT_NULL(strstr(snap.status, "SMP"));
}

// Once a seed is explicitly assigned an engine we expect that choice to survive
// across reseeds and for the PatternScheduler mirror to stay in sync. This test
// walks through that exact ritual.
void test_engine_selection_persists_and_updates_scheduler() {
  AppState app;
  app.initSim();

  app.setSeedEngine(1, 2);
  TEST_ASSERT_EQUAL_UINT8(2, app.seeds()[1].engine);

  const Seed* scheduled = app.debugScheduledSeed(1);
  TEST_ASSERT_NOT_NULL(scheduled);
  TEST_ASSERT_EQUAL_UINT8(2, scheduled->engine);

  const uint32_t baselineSeed = app.masterSeed();
  app.reseed(baselineSeed);

  TEST_ASSERT_EQUAL_UINT8(2, app.seeds()[1].engine);
  scheduled = app.debugScheduledSeed(1);
  TEST_ASSERT_NOT_NULL(scheduled);
  TEST_ASSERT_EQUAL_UINT8(2, scheduled->engine);
}

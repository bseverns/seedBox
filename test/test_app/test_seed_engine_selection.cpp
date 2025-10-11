#include <unity.h>
#include <cstring>
#include "app/AppState.h"

namespace {
constexpr uint8_t kEngineCycleCc = 20;
}

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

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_cc_cycles_engine_and_snapshot_updates);
  RUN_TEST(test_engine_selection_persists_and_updates_scheduler);
  return UNITY_END();
}

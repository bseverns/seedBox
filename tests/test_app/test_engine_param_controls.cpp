#include <unity.h>

#include "app/AppState.h"
#include "hal/Board.h"
#include "hal/hal_io.h"

namespace {
constexpr int kBurstSpacingStepSamples = 240;

void runTicks(AppState& app, int count) {
  for (int i = 0; i < count; ++i) {
    app.tick();
  }
}
}

void test_engine_mode_twiddles_euclid_and_burst() {
  hal::nativeBoardReset();
  AppState app;
  app.initSim();

  app.setSeedEngine(app.focusSeed(), EngineRouter::kEuclidId);
  app.setModeFromHost(AppState::Mode::ENGINE);

  const auto baseSteps = app.engineRouterForDebug().euclid().steps();
  hal::nativeBoardFeed("enc density 1");
  runTicks(app, 6);
  TEST_ASSERT_EQUAL_UINT8(baseSteps + 1, app.engineRouterForDebug().euclid().steps());

  const auto baseFills = app.engineRouterForDebug().euclid().fills();
  hal::nativeBoardFeed("enc tone 2");
  runTicks(app, 6);
  TEST_ASSERT_EQUAL_UINT8(baseFills + 2, app.engineRouterForDebug().euclid().fills());

  const auto baseRotate = app.engineRouterForDebug().euclid().rotate();
  const auto stepsNow = app.engineRouterForDebug().euclid().steps();
  hal::nativeBoardFeed("enc fx -1");
  runTicks(app, 6);
  const uint8_t expectedRotate = static_cast<uint8_t>((stepsNow + baseRotate - 1) % stepsNow);
  TEST_ASSERT_EQUAL_UINT8(expectedRotate, app.engineRouterForDebug().euclid().rotate());

  app.setSeedEngine(app.focusSeed(), EngineRouter::kBurstId);
  app.setModeFromHost(AppState::Mode::ENGINE);

  const auto baseClusters = app.engineRouterForDebug().burst().clusterCount();
  hal::nativeBoardFeed("enc density 1");
  runTicks(app, 6);
  TEST_ASSERT_EQUAL_UINT8(baseClusters + 1, app.engineRouterForDebug().burst().clusterCount());

  const auto baseSpacing = app.engineRouterForDebug().burst().spacingSamples();
  hal::nativeBoardFeed("enc tone -1");
  runTicks(app, 6);
  const auto expectedSpacing = (baseSpacing > kBurstSpacingStepSamples)
                                   ? (baseSpacing - kBurstSpacingStepSamples)
                                   : 0u;
  TEST_ASSERT_EQUAL_UINT32(expectedSpacing, app.engineRouterForDebug().burst().spacingSamples());

  app.seedPageToggleLock(app.focusSeed());
  hal::nativeBoardFeed("enc tone 1");
  runTicks(app, 6);
  TEST_ASSERT_EQUAL_UINT32(expectedSpacing, app.engineRouterForDebug().burst().spacingSamples());
}

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_engine_mode_twiddles_euclid_and_burst);
  return UNITY_END();
}

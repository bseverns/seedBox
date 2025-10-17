#include <unity.h>
#include "app/AppState.h"
#include "interop/mn42_map.h"

void test_external_clock_priority() {
  AppState app;
  app.initSim();

  const uint64_t ticksBefore = app.schedulerTicks();
  app.tick();
  TEST_ASSERT_TRUE(app.schedulerTicks() > ticksBefore);

  const uint64_t internalBaseline = app.schedulerTicks();
  app.onExternalTransportStart();
  app.tick();
  TEST_ASSERT_TRUE(app.schedulerTicks() > internalBaseline);

  const uint8_t mn42Channel = static_cast<uint8_t>(seedbox::interop::mn42::kDefaultChannel + 1);
  app.onExternalControlChange(mn42Channel,
                              seedbox::interop::mn42::cc::kMode,
                              seedbox::interop::mn42::mode::kFollowExternalClock);

  const uint64_t followBaseline = app.schedulerTicks();
  app.onExternalTransportStart();
  app.tick();
  TEST_ASSERT_EQUAL_UINT64(followBaseline, app.schedulerTicks());

  app.onExternalClockTick();
  TEST_ASSERT_TRUE(app.schedulerTicks() > followBaseline);

  const uint64_t externalBaseline = app.schedulerTicks();
  app.tick();
  TEST_ASSERT_EQUAL_UINT64(externalBaseline, app.schedulerTicks());

  app.onExternalTransportStop();
  app.tick();
  TEST_ASSERT_EQUAL_UINT64(externalBaseline, app.schedulerTicks());

  app.onExternalControlChange(mn42Channel,
                              seedbox::interop::mn42::cc::kMode,
                              0);
  app.tick();
  TEST_ASSERT_TRUE(app.schedulerTicks() > externalBaseline);
}

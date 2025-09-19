#include <unity.h>
#include "app/AppState.h"

void test_external_clock_priority() {
  AppState app;
  app.initSim();

  const uint64_t ticksBefore = app.schedulerTicks();
  app.tick();
  TEST_ASSERT_TRUE(app.schedulerTicks() > ticksBefore);

  const uint64_t internalBaseline = app.schedulerTicks();
  app.onExternalTransportStart();
  app.tick();
  TEST_ASSERT_EQUAL_UINT64(internalBaseline, app.schedulerTicks());

  app.onExternalClockTick();
  TEST_ASSERT_TRUE(app.schedulerTicks() > internalBaseline);

  const uint64_t externalBaseline = app.schedulerTicks();
  app.tick();
  TEST_ASSERT_EQUAL_UINT64(externalBaseline, app.schedulerTicks());

  app.onExternalTransportStop();
  app.tick();
  TEST_ASSERT_TRUE(app.schedulerTicks() > externalBaseline);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_external_clock_priority);
  return UNITY_END();
}

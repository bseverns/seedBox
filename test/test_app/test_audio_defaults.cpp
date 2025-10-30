#include <cstring>
#include <unity.h>

#include "app/AppState.h"
#include "hal/hal_audio.h"
#include "util/Units.h"

// The simulator should boot the audio HAL at 48 kHz so every tempo/math helper
// agrees on what "one second" means. This test doubles as a breadcrumb for the
// classroom demos that lean on AppState's display snapshot.
void test_simulator_audio_reports_48k() {
  AppState app;
  app.initSim();

  TEST_ASSERT_FLOAT_WITHIN(0.01f, Units::kSampleRate, hal::audio::sampleRate());

  AppState::DisplaySnapshot snap{};
  app.captureDisplaySnapshot(snap);
  TEST_ASSERT_NOT_NULL(strstr(snap.metrics, "SR48.0k"));
}

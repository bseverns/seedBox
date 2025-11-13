#include <cstring>
#include <unity.h>

#include "app/AppState.h"
#include "hal/hal_audio.h"
#include "util/Units.h"

// The simulator exposes dial-a-rate hooks that let the tests lock the audio HAL
// to deterministic sample rates.  Hardware has to accept whatever the codec is
// clocked at, so we fall back to a skipped stub there instead of triggering a
// compile-time failure.
#if !SEEDBOX_HW

// The simulator should boot the audio HAL at 48 kHz so every tempo/math helper
// agrees on what "one second" means. This test doubles as a breadcrumb for the
// classroom demos that lean on AppState's display snapshot.
void test_simulator_audio_reports_48k() {
  // Start from a known state so we don't inherit whatever previous tests did to
  // the HAL's globals. The simulator surface lets us hand-patch the rate.
  hal::audio::mockSetSampleRate(Units::kSampleRate);

  AppState app;

  // Before the runtime seeds itself the OLED should advertise the raw audio
  // plumbing stats. That includes the simulator's canonical 48 kHz.
  AppState::DisplaySnapshot snap{};
  app.captureDisplaySnapshot(snap);
  TEST_ASSERT_NOT_NULL_MESSAGE(strstr(snap.metrics, "SR48.0k"), snap.metrics);

  // Once the sim boots the sample rate should stay welded at the same 48 kHz
  // so tempo math remains deterministic for tests and lecture demos.
  app.initSim();
  TEST_ASSERT_FLOAT_WITHIN(0.01f, Units::kSampleRate, hal::audio::sampleRate());
}

#else  // SEEDBOX_HW

void test_simulator_audio_reports_48k() {
  TEST_IGNORE_MESSAGE("audio sample-rate probe only applies to the simulator");
}

#endif  // !SEEDBOX_HW

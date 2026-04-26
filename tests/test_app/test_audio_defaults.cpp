#include <algorithm>
#include <cstring>
#include <unity.h>

#include "app/AppState.h"
#include "engine/EngineRouter.h"
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

void test_engine_idle_floor_tracks_peak_and_rms() {
  constexpr std::size_t kFrames = 8;
  float silence[kFrames]{};
  TEST_ASSERT_TRUE(hal::audio::bufferEngineIdle(silence, nullptr, kFrames));

  float rightOnly[kFrames]{};
  rightOnly[3] = 2.0e-5f;  // poke above the idle floor on the right channel only
  TEST_ASSERT_FALSE(hal::audio::bufferEngineIdle(silence, rightOnly, kFrames));

  float stereoFuzz[kFrames];
  std::fill(std::begin(stereoFuzz), std::end(stereoFuzz), 2.0e-6f);
  TEST_ASSERT_TRUE(hal::audio::bufferEngineIdle(stereoFuzz, stereoFuzz, kFrames));
}

void test_juce_host_render_changes_live_input_when_granular_seed_is_focused() {
  constexpr std::size_t kFrames = 128;
  AppState app;
  app.initJuceHost(Units::kSampleRate, kFrames);

  AppState::StatusSnapshot status{};
  app.captureStatusSnapshot(status);
  TEST_ASSERT_EQUAL_UINT8_MESSAGE(EngineRouter::kGranularId, status.focusSeedEngineId,
                                  "JUCE boot preset did not focus the granular engine");

  std::vector<float> input(kFrames, 0.0f);
  for (std::size_t i = 0; i < input.size(); ++i) {
    input[i] = (i % 16u < 8u) ? 0.35f : -0.35f;
  }

  std::vector<float> left(kFrames, 0.0f);
  std::vector<float> right(kFrames, 0.0f);
  for (int pass = 0; pass < 4; ++pass) {
    app.setDryInputFromHost(input.data(), nullptr, input.size());
    hal::audio::renderHostBuffer(left.data(), right.data(), left.size());
  }

  float diff = 0.0f;
  float energy = 0.0f;
  for (std::size_t i = 0; i < input.size(); ++i) {
    diff += std::abs(left[i] - input[i]);
    energy += std::abs(left[i]);
  }

  char diffMsg[96];
  std::snprintf(diffMsg, sizeof(diffMsg), "host render matched dry input too closely (diff=%.6f)", diff);
  char energyMsg[96];
  std::snprintf(energyMsg, sizeof(energyMsg), "host render stayed effectively silent (energy=%.6f)", energy);

  TEST_ASSERT_TRUE_MESSAGE(diff > 0.5f, diffMsg);
  TEST_ASSERT_TRUE_MESSAGE(energy > 0.5f, energyMsg);
}

void test_juce_host_render_boot_seeds_remain_audible_when_focus_changes() {
  constexpr std::size_t kFrames = 128;
  AppState app;
  app.initJuceHost(Units::kSampleRate, kFrames);

  std::vector<float> input(kFrames, 0.0f);
  for (std::size_t i = 0; i < input.size(); ++i) {
    input[i] = (i % 12u < 6u) ? 0.28f : -0.24f;
  }

  std::vector<float> left(kFrames, 0.0f);
  std::vector<float> right(kFrames, 0.0f);
  for (std::uint8_t focus = 0; focus < 4; ++focus) {
    app.setFocusSeed(focus);

    float diff = 0.0f;
    float energy = 0.0f;
    float peak = 0.0f;
    for (int pass = 0; pass < 6; ++pass) {
      app.setDryInputFromHost(input.data(), nullptr, input.size());
      hal::audio::renderHostBuffer(left.data(), right.data(), left.size());
      for (std::size_t i = 0; i < input.size(); ++i) {
        diff += std::abs(left[i] - input[i]);
        energy += std::abs(left[i]);
        peak = std::max(peak, std::abs(left[i]));
        peak = std::max(peak, std::abs(right[i]));
      }
    }

    char diffMsg[96];
    std::snprintf(diffMsg, sizeof(diffMsg), "focus seed %u stayed too close to dry input (diff=%.6f)",
                  static_cast<unsigned>(focus), diff);
    char energyMsg[96];
    std::snprintf(energyMsg, sizeof(energyMsg), "focus seed %u stayed effectively silent (energy=%.6f)",
                  static_cast<unsigned>(focus), energy);
    char peakMsg[96];
    std::snprintf(peakMsg, sizeof(peakMsg), "focus seed %u clipped too hard (peak=%.6f)",
                  static_cast<unsigned>(focus), peak);

    TEST_ASSERT_TRUE_MESSAGE(diff > 0.5f, diffMsg);
    TEST_ASSERT_TRUE_MESSAGE(energy > 0.5f, energyMsg);
    TEST_ASSERT_TRUE_MESSAGE(peak < 0.98f, peakMsg);
  }
}

void test_juce_host_arms_granular_live_input_for_effect_processing() {
  AppState app;
  app.initJuceHost(Units::kSampleRate, 128);

  auto seed = app.seeds().front();
  seed.id = 0;
  seed.engine = EngineRouter::kGranularId;
  seed.granular.source = static_cast<std::uint8_t>(GranularEngine::Source::kLiveInput);
  seed.granular.sdSlot = 0;

  auto& granular = app.engineRouterForDebug().granular();
  granular.trigger(seed, 0);

  const auto voice = granular.voice(0);
  TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(GranularEngine::Source::kLiveInput),
                          static_cast<std::uint8_t>(voice.source));
}

#else  // SEEDBOX_HW

void test_simulator_audio_reports_48k() {
  TEST_IGNORE_MESSAGE("audio sample-rate probe only applies to the simulator");
}

void test_engine_idle_floor_tracks_peak_and_rms() {
  TEST_IGNORE_MESSAGE("engine idle floor check only applies to the simulator");
}

void test_juce_host_render_changes_live_input_when_granular_seed_is_focused() {
  TEST_IGNORE_MESSAGE("JUCE host render check only applies to the simulator");
}

void test_juce_host_render_boot_seeds_remain_audible_when_focus_changes() {
  TEST_IGNORE_MESSAGE("JUCE host render focus-switch check only applies to the simulator");
}

void test_juce_host_arms_granular_live_input_for_effect_processing() {
  TEST_IGNORE_MESSAGE("JUCE host render granular live-input check only applies to the simulator");
}

#endif  // !SEEDBOX_HW

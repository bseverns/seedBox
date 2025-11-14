#include <unity.h>

#include <array>

#include "app/AppState.h"
#include "engine/EngineRouter.h"
#include "engine/Granular.h"
#include "hal/Board.h"

void test_live_input_prime_tags_seeds_as_live() {
  AppState app;
  app.initSim();

  const uint32_t startSeed = app.masterSeed();
  app.seedPageReseed(startSeed, AppState::SeedPrimeMode::kLiveInput);

  TEST_ASSERT_EQUAL_UINT32(startSeed, app.masterSeed());
  TEST_ASSERT_EQUAL(static_cast<int>(AppState::SeedPrimeMode::kLiveInput),
                    static_cast<int>(app.seedPrimeMode()));

  const auto& seeds = app.seeds();
  TEST_ASSERT_FALSE(seeds.empty());
  for (const auto& seed : seeds) {
    TEST_ASSERT_EQUAL(static_cast<int>(Seed::Source::kLiveInput), static_cast<int>(seed.source));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(GranularEngine::Source::kLiveInput), seed.granular.source);
    TEST_ASSERT_EQUAL_UINT8(0, seed.granular.sdSlot);
    TEST_ASSERT_EQUAL_UINT32(startSeed, seed.lineage);
  }
}

void test_live_input_prime_respects_all_engine_assignments() {
  AppState app;
  app.initSim();

  const std::array<uint8_t, 5> engineIds = {
      EngineRouter::kSamplerId,
      EngineRouter::kGranularId,
      EngineRouter::kResonatorId,
      EngineRouter::kEuclidId,
      EngineRouter::kBurstId,
  };

  for (uint8_t engineId : engineIds) {
    app.setSeedEngine(0, engineId);
    app.seedPageReseed(app.masterSeed(), AppState::SeedPrimeMode::kLiveInput);

    const auto& seeds = app.seeds();
    TEST_ASSERT_FALSE(seeds.empty());
    TEST_ASSERT_EQUAL_UINT8(engineId, seeds.front().engine);
    TEST_ASSERT_EQUAL(static_cast<int>(Seed::Source::kLiveInput), static_cast<int>(seeds.front().source));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(GranularEngine::Source::kLiveInput), seeds.front().granular.source);
  }
}

void test_live_input_prime_triggers_live_voice_in_sim() {
  hal::nativeBoardReset();
  AppState app;
  app.initSim();
  app.armGranularLiveInput(true);

  const uint32_t startSeed = app.masterSeed();
  app.seedPageReseed(startSeed, AppState::SeedPrimeMode::kLiveInput);

  app.setSeedEngine(0, EngineRouter::kGranularId);

  TEST_ASSERT_FALSE(app.seeds().empty());
  const Seed& first = app.seeds().front();
  AppState::SeedNudge boost{};
  boost.densityDelta = 24.f;  // supercharge hits per beat so the scheduler fires fast
  boost.probabilityDelta = 1.0f - first.probability;
  boost.jitterDeltaMs = -first.jitterMs;
  app.seedPageNudge(0, boost);

  for (int tick = 0; tick < 96; ++tick) {
    app.tick();
  }

  const auto voice = app.debugGranularVoice(0);
  TEST_ASSERT_TRUE_MESSAGE(voice.active, "Granular voice should be active after a live-input prime reseed");
  TEST_ASSERT_EQUAL(static_cast<int>(GranularEngine::Source::kLiveInput), static_cast<int>(voice.source));
  TEST_ASSERT_NOT_NULL(voice.sourcePath);
  TEST_ASSERT_EQUAL_STRING("live-in", voice.sourcePath);

#if !SEEDBOX_HW
  const auto simVoice = app.debugGranularSimVoice(0);
  TEST_ASSERT_TRUE(simVoice.sdPlayerStopCalled);
  TEST_ASSERT_FALSE(simVoice.sdPlayerPlayCalled);
  TEST_ASSERT_FALSE(simVoice.sdPlayerPlaying);
#endif
}

#include <unity.h>

#include <array>

#include "app/AppState.h"
#include "engine/EngineRouter.h"
#include "engine/Granular.h"

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

#include <unity.h>

#include <array>
#include <algorithm>
#include <vector>

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

void test_tap_tempo_prime_updates_lineage() {
  AppState app;
  app.initSim();

  constexpr uint32_t kMasterSeed = 0x5EEDC0DEu;
  app.recordTapTempoInterval(500);
  app.recordTapTempoInterval(520);
  app.recordTapTempoInterval(480);

  app.seedPageReseed(kMasterSeed, AppState::SeedPrimeMode::kTapTempo);
  const auto firstPass = app.seeds();
  TEST_ASSERT_FALSE(firstPass.empty());

  const double averageMs = (500.0 + 520.0 + 480.0) / 3.0;
  const float expectedBpm = static_cast<float>(60000.0 / averageMs);
  const uint32_t expectedLineage =
      static_cast<uint32_t>(std::max(0.f, expectedBpm * 100.f));

  for (const auto& seed : firstPass) {
    TEST_ASSERT_EQUAL(static_cast<int>(Seed::Source::kTapTempo),
                      static_cast<int>(seed.source));
    TEST_ASSERT_EQUAL_UINT32(expectedLineage, seed.lineage);
  }

  app.seedPageReseed(kMasterSeed, AppState::SeedPrimeMode::kTapTempo);
  const auto secondPass = app.seeds();
  TEST_ASSERT_EQUAL(firstPass.size(), secondPass.size());

  for (std::size_t i = 0; i < firstPass.size(); ++i) {
    const auto& a = firstPass[i];
    const auto& b = secondPass[i];
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, a.granular.grainSizeMs, b.granular.grainSizeMs);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, a.granular.sprayMs, b.granular.sprayMs);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, a.granular.transpose, b.granular.transpose);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, a.granular.windowSkew, b.granular.windowSkew);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, a.granular.stereoSpread, b.granular.stereoSpread);
    TEST_ASSERT_EQUAL_UINT8(a.granular.source, b.granular.source);
    TEST_ASSERT_EQUAL_UINT8(a.granular.sdSlot, b.granular.sdSlot);
  }
}

void test_preset_prime_applies_granular_params() {
  AppState app;
  app.initSim();

  constexpr uint32_t kPresetId = 0x5EEDFA11u;
  std::vector<Seed> presetSeeds;
  constexpr std::size_t kSeedCount = 4;
  presetSeeds.reserve(kSeedCount);
  for (std::size_t i = 0; i < kSeedCount; ++i) {
    Seed seed{};
    seed.id = static_cast<uint32_t>(i);
    seed.engine = (i % 2 == 0) ? EngineRouter::kSamplerId : EngineRouter::kGranularId;
    seed.granular.grainSizeMs = 40.f + static_cast<float>(i * 7);
    seed.granular.sprayMs = 5.f + static_cast<float>(i);
    seed.granular.transpose = static_cast<float>(static_cast<int32_t>(i) - 1);
    seed.granular.windowSkew = -0.25f + 0.25f * static_cast<float>(i);
    seed.granular.stereoSpread = 0.2f + 0.15f * static_cast<float>(i);
    if (i % 2 == 0) {
      seed.granular.source = static_cast<uint8_t>(GranularEngine::Source::kSdClip);
      seed.granular.sdSlot = static_cast<uint8_t>(i + 1);
    } else {
      seed.granular.source = static_cast<uint8_t>(GranularEngine::Source::kLiveInput);
      seed.granular.sdSlot = 0;
    }
    presetSeeds.push_back(seed);
  }
  app.setSeedPreset(kPresetId, presetSeeds);

  app.seedPageReseed(app.masterSeed(), AppState::SeedPrimeMode::kPreset);
  const std::vector<bool> expectedLocks = {false, true, false, true};
  for (std::size_t i = 0; i < expectedLocks.size(); ++i) {
    if (expectedLocks[i]) {
      app.seedPageToggleLock(static_cast<uint8_t>(i));
    }
    app.setSeedEngine(static_cast<uint8_t>(i), presetSeeds[i].engine);
  }

  for (std::size_t i = 0; i < presetSeeds.size(); ++i) {
    if (!expectedLocks[i]) {
      app.seedPageCycleGranularSource(static_cast<uint8_t>(i), 1);
    }
  }

  const auto mutated = app.seeds();
  for (std::size_t i = 0; i < mutated.size(); ++i) {
    if (!expectedLocks[i]) {
      TEST_ASSERT_TRUE((mutated[i].granular.source != presetSeeds[i].granular.source) ||
                       (mutated[i].granular.sdSlot != presetSeeds[i].granular.sdSlot));
    }
  }

  app.seedPageReseed(app.masterSeed(), AppState::SeedPrimeMode::kPreset);
  const auto reseeded = app.seeds();
  TEST_ASSERT_EQUAL(presetSeeds.size(), reseeded.size());

  for (std::size_t i = 0; i < reseeded.size(); ++i) {
    TEST_ASSERT_EQUAL(static_cast<int>(Seed::Source::kPreset),
                      static_cast<int>(reseeded[i].source));
    TEST_ASSERT_EQUAL_UINT32(kPresetId, reseeded[i].lineage);
    TEST_ASSERT_EQUAL_UINT8(presetSeeds[i].granular.source, reseeded[i].granular.source);
    TEST_ASSERT_EQUAL_UINT8(presetSeeds[i].granular.sdSlot, reseeded[i].granular.sdSlot);
    TEST_ASSERT_EQUAL(expectedLocks[i], app.isSeedLocked(static_cast<uint8_t>(i)));
  }
}

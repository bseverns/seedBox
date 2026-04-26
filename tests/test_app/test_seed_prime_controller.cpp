#include <unity.h>

#include <vector>

#include "app/SeedPrimeController.h"
#include "engine/EngineRouter.h"
#include "engine/Granular.h"

void test_seed_prime_controller_rotates_modes_and_labels() {
  SeedPrimeController controller;

  TEST_ASSERT_EQUAL(static_cast<int>(SeedPrimeController::Mode::kTapTempo),
                    static_cast<int>(controller.rotateMode(SeedPrimeController::Mode::kLfsr, 1)));
  TEST_ASSERT_EQUAL(static_cast<int>(SeedPrimeController::Mode::kLiveInput),
                    static_cast<int>(controller.rotateMode(SeedPrimeController::Mode::kLfsr, -1)));
  TEST_ASSERT_EQUAL_STRING("LFSR", controller.modeLabel(SeedPrimeController::Mode::kLfsr));
  TEST_ASSERT_EQUAL_STRING("Tap", controller.modeLabel(SeedPrimeController::Mode::kTapTempo));
  TEST_ASSERT_EQUAL_STRING("Preset", controller.modeLabel(SeedPrimeController::Mode::kPreset));
  TEST_ASSERT_EQUAL_STRING("Live", controller.modeLabel(SeedPrimeController::Mode::kLiveInput));
}

void test_seed_prime_controller_builds_lfsr_seeds() {
  SeedPrimeController controller;
  constexpr uint32_t kMasterSeed = 0x12345678u;
  const auto seeds = controller.buildSeeds(SeedPrimeController::Mode::kLfsr, kMasterSeed, 4, 0.4f, 0.15f,
                                           120.f, 0u, 0u, {}, 0u);

  TEST_ASSERT_EQUAL_UINT32(4u, static_cast<uint32_t>(seeds.size()));
  for (std::size_t i = 0; i < seeds.size(); ++i) {
    const Seed& seed = seeds[i];
    TEST_ASSERT_EQUAL(static_cast<int>(Seed::Source::kLfsr), static_cast<int>(seed.source));
    TEST_ASSERT_EQUAL_UINT32(kMasterSeed, seed.lineage);
    TEST_ASSERT_EQUAL_UINT32(static_cast<uint32_t>(i), seed.id);
    TEST_ASSERT_NOT_EQUAL(0u, seed.prng);
  }
}

void test_seed_prime_controller_builds_tap_preset_and_live_seeds() {
  SeedPrimeController controller;
  constexpr uint32_t kMasterSeed = 0x5EEDC0DEu;

  const auto tapSeeds = controller.buildSeeds(SeedPrimeController::Mode::kTapTempo, kMasterSeed, 2, 0.35f, 0.2f,
                                              150.f, 0u, 0u, {}, 0u);
  TEST_ASSERT_EQUAL_UINT32(2u, static_cast<uint32_t>(tapSeeds.size()));
  TEST_ASSERT_EQUAL(static_cast<int>(Seed::Source::kTapTempo), static_cast<int>(tapSeeds.front().source));
  TEST_ASSERT_EQUAL_UINT32(15000u, tapSeeds.front().lineage);

  Seed preset{};
  preset.id = 9;
  preset.engine = EngineRouter::kGranularId;
  preset.granular.source = static_cast<uint8_t>(GranularEngine::Source::kSdClip);
  preset.granular.sdSlot = 2;
  preset.probability = 0.75f;
  preset.pitch = 7.0f;
  std::vector<Seed> presetBank{preset};
  const auto presetSeeds = controller.buildSeeds(SeedPrimeController::Mode::kPreset, kMasterSeed, 3, 0.4f, 0.1f,
                                                 120.f, 0u, 0u, presetBank, 0xCAFEu);
  TEST_ASSERT_EQUAL_UINT32(3u, static_cast<uint32_t>(presetSeeds.size()));
  TEST_ASSERT_EQUAL(static_cast<int>(Seed::Source::kPreset), static_cast<int>(presetSeeds.front().source));
  TEST_ASSERT_EQUAL_UINT32(0xCAFEu, presetSeeds.front().lineage);
  TEST_ASSERT_EQUAL_UINT8(preset.granular.source, presetSeeds.front().granular.source);
  TEST_ASSERT_EQUAL_UINT8(preset.granular.sdSlot, presetSeeds.front().granular.sdSlot);
  TEST_ASSERT_EQUAL_UINT8(preset.engine, presetSeeds.front().engine);

  const auto liveSeeds = controller.buildSeeds(SeedPrimeController::Mode::kLiveInput, kMasterSeed, 2, 0.4f, 0.1f,
                                               120.f, 0xABCD1234u, 3u, {}, 0u);
  TEST_ASSERT_EQUAL_UINT32(2u, static_cast<uint32_t>(liveSeeds.size()));
  TEST_ASSERT_EQUAL(static_cast<int>(Seed::Source::kLiveInput), static_cast<int>(liveSeeds.front().source));
  TEST_ASSERT_EQUAL_UINT32(0xABCD1234u, liveSeeds.front().lineage);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(GranularEngine::Source::kLiveInput), liveSeeds.front().granular.source);
  TEST_ASSERT_EQUAL_UINT8(3u, liveSeeds.front().granular.sdSlot);
  TEST_ASSERT_EQUAL_UINT8(3u, liveSeeds.front().sampleIdx);
}

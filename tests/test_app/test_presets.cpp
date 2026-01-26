#include <algorithm>
#include <unity.h>

#include "app/AppState.h"
#include "hal/Board.h"
#include "io/Store.h"

namespace {
constexpr uint32_t kSeedCapture = 0x12345678u;
constexpr uint32_t kAlternativeSeed = 0x5EEDCAFEu;
}

#include "Seed.h"

namespace {
void runTicks(AppState& app, int count) {
  for (int i = 0; i < count; ++i) {
    app.tick();
  }
}
}  // namespace

void test_preset_round_trip_via_eeprom_store() {
  AppState app;
  seedbox::io::StoreEeprom store(2048);
  app.attachStore(&store);
  app.initSim();
  app.setPage(AppState::Page::kStorage);

  app.reseed(kSeedCapture);
  const auto beforeSeeds = app.seeds();
  TEST_ASSERT_FALSE(beforeSeeds.empty());
  TEST_ASSERT_TRUE(app.savePreset("alpha"));
  TEST_ASSERT_EQUAL_STRING("alpha", app.activePresetSlot().c_str());

  const auto names = app.storedPresets();
  TEST_ASSERT_TRUE(std::find(names.begin(), names.end(), "alpha") != names.end());

  app.reseed(kAlternativeSeed);
  TEST_ASSERT_TRUE(app.recallPreset("alpha", false));
  runTicks(app, 1);
  TEST_ASSERT_EQUAL_UINT32(kSeedCapture, app.masterSeed());
  const auto afterSeeds = app.seeds();
  TEST_ASSERT_EQUAL(beforeSeeds.size(), afterSeeds.size());
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, beforeSeeds[0].pitch, afterSeeds[0].pitch);

  app.reseed(kAlternativeSeed);
  TEST_ASSERT_TRUE(app.recallPreset("alpha", true));
  for (std::uint32_t i = 0; i < AppState::kPresetCrossfadeTicks; ++i) {
    app.tick();
  }
  const auto blendedSeeds = app.seeds();
  TEST_ASSERT_EQUAL(beforeSeeds.size(), blendedSeeds.size());
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, beforeSeeds[0].granular.grainSizeMs,
                           blendedSeeds[0].granular.grainSizeMs);
}

void test_init_sim_attaches_default_store() {
  AppState app;
  app.initSim();

  TEST_ASSERT_NOT_NULL(app.store());
  const auto names = app.storedPresets();
  TEST_ASSERT_TRUE(names.empty());
}

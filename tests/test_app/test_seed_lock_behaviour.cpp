#include <unity.h>

#include "app/AppState.h"
#include "interop/mn42_map.h"

namespace {
constexpr uint8_t kSeedCount = 4;
}

void test_seed_lock_survives_reseed_and_engine_swap() {
  AppState app;
  app.initSim();

  TEST_ASSERT_EQUAL_UINT8(kSeedCount, static_cast<uint8_t>(app.seeds().size()));

  const uint8_t focus = app.focusSeed();
  const Seed originalLocked = app.seeds()[1];
  const Seed originalUnlocked = app.seeds()[0];

  app.setSeedEngine(1, 2);
  app.seedPageToggleLock(1);
  TEST_ASSERT_TRUE(app.isSeedLocked(1));

  const uint32_t newSeed = app.masterSeed() + 17u;
  app.reseed(newSeed);

  TEST_ASSERT_EQUAL_UINT8(kSeedCount, static_cast<uint8_t>(app.seeds().size()));
  const Seed& locked = app.seeds()[1];
  TEST_ASSERT_EQUAL_FLOAT(originalLocked.pitch, locked.pitch);
  TEST_ASSERT_EQUAL_FLOAT(originalLocked.density, locked.density);
  TEST_ASSERT_EQUAL_UINT32(originalLocked.prng, locked.prng);
  TEST_ASSERT_EQUAL_UINT8(2, locked.engine);

  const Seed& unlocked = app.seeds()[0];
  bool unlockedChanged = (unlocked.prng != originalUnlocked.prng) ||
                         (unlocked.pitch != originalUnlocked.pitch) ||
                         (unlocked.density != originalUnlocked.density);
  TEST_ASSERT_TRUE(unlockedChanged);

  const Seed* resonatorSeed = app.engineRouterForDebug().resonator().lastSeed(1);
  TEST_ASSERT_NOT_NULL(resonatorSeed);
  TEST_ASSERT_EQUAL_UINT8(2, resonatorSeed->engine);
  TEST_ASSERT_EQUAL_FLOAT(locked.pitch, resonatorSeed->pitch);

  // Ensure focus survived sane bounds after reseed.
  TEST_ASSERT_LESS_OR_EQUAL_UINT(app.seeds().size() - 1, app.focusSeed());
  (void)focus;
}

void test_global_lock_blocks_reseed_changes() {
  AppState app;
  app.initSim();

  auto snapshot = app.seeds();
  app.seedPageToggleGlobalLock();
  TEST_ASSERT_TRUE(app.isGlobalSeedLocked());

  app.reseed(app.masterSeed() + 101u);

  TEST_ASSERT_EQUAL(snapshot.size(), app.seeds().size());
  for (std::size_t i = 0; i < snapshot.size(); ++i) {
    TEST_ASSERT_EQUAL_FLOAT(snapshot[i].pitch, app.seeds()[i].pitch);
    TEST_ASSERT_EQUAL_UINT32(snapshot[i].prng, app.seeds()[i].prng);
  }

  app.seedPageToggleGlobalLock();
  TEST_ASSERT_FALSE(app.isGlobalSeedLocked());
}

void test_quantize_control_snaps_pitch_to_scale() {
  AppState app;
  app.initSim();

  const uint8_t focus = app.focusSeed();
  const float desired = 0.37f;
  AppState::SeedNudge nudge{};
  nudge.pitchSemitones = desired - app.seeds()[focus].pitch;
  app.seedPageNudge(focus, nudge);
  TEST_ASSERT_FLOAT_WITHIN(1e-4f, desired, app.seeds()[focus].pitch);

  app.onExternalControlChange(seedbox::interop::mn42::kDefaultChannel,
                              seedbox::interop::mn42::cc::kQuantize, 32);

  const Seed& quantized = app.seeds()[focus];
  TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.f, quantized.pitch);

  const Seed* samplerSeed = app.engineRouterForDebug().sampler().lastSeed(focus);
  TEST_ASSERT_NOT_NULL(samplerSeed);
  TEST_ASSERT_FLOAT_WITHIN(1e-4f, quantized.pitch, samplerSeed->pitch);
}


#include <unity.h>

#include <cstdint>

#include "app/AppState.h"

void test_seed_prime_bypass_leaves_slots_empty_and_ignores_locks() {
  AppState app;
  app.setSeedPrimeBypassFromHost(true);
  app.initSim();

  app.reseed(0x12345678u);
  const auto& seeds = app.seeds();

  TEST_ASSERT_EQUAL_size_t(4, seeds.size());
  const uint8_t focus = app.focusSeed();

  for (std::size_t i = 0; i < seeds.size(); ++i) {
    if (i == focus) {
      TEST_ASSERT_NOT_EQUAL_UINT32(0u, seeds[i].prng);
    } else {
      TEST_ASSERT_EQUAL_UINT32(0u, seeds[i].prng);
    }
  }

  app.seedPageToggleLock(focus);
  const uint32_t lockedPrng = app.seeds()[focus].prng;

  app.reseed(0x87654321u);
  TEST_ASSERT_NOT_EQUAL_UINT32(lockedPrng, app.seeds()[focus].prng);

  for (std::size_t i = 0; i < app.seeds().size(); ++i) {
    if (i == focus) {
      continue;
    }
    TEST_ASSERT_EQUAL_UINT32(0u, app.seeds()[i].prng);
  }
}


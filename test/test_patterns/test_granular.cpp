#include <unity.h>

// We break encapsulation in tests so we can poke the grain planner directly
// and verify the deterministic jitter math. Punk-rock, but effective.
#define private public
#include "engine/Granular.h"
#undef private

#include "Seed.h"
#include "util/RNG.h"
#include "util/Units.h"
#include <cmath>

void test_plan_grain_sprays_and_mutates_prng() {
  GranularEngine engine;
  engine.init(GranularEngine::Mode::kSim);

  GranularEngine::GrainVoice voice{};
  Seed seed{};
  seed.id = 7;
  seed.prng = 12345u;
  seed.granular.sprayMs = 12.5f;

  const uint32_t whenSamples = 2048u;
  engine.planGrain(voice, seed, whenSamples);

  uint32_t expectedPrng = seed.prng;
  uint32_t expectedStart = whenSamples;
  {
    float spray = RNG::uniformSigned(expectedPrng) * seed.granular.sprayMs;
    const uint32_t offset = Units::msToSamples(std::abs(spray));
    if (spray >= 0.f) {
      expectedStart += offset;
    } else {
      expectedStart = (expectedStart > offset) ? (expectedStart - offset) : 0u;
    }
  }

  TEST_ASSERT_EQUAL_UINT32(expectedPrng, voice.seedPrng);
  TEST_ASSERT_EQUAL_UINT32(expectedStart, voice.startSample);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(seed.id), voice.seedId);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_plan_grain_sprays_and_mutates_prng);
  return UNITY_END();
}

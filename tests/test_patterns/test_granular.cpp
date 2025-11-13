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

void test_map_grain_honors_stereo_spread_width_curve() {
  GranularEngine engine;
  engine.init(GranularEngine::Mode::kSim);

  GranularEngine::GrainVoice mono{};
  mono.stereoSpread = 0.0f;
  engine.mapGrainToGraph(0, mono);
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, mono.leftGain, mono.rightGain);
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.70710677f, mono.leftGain);

  GranularEngine::GrainVoice wide{};
  wide.stereoSpread = 1.0f;
  engine.mapGrainToGraph(0, wide);
  TEST_ASSERT_TRUE(wide.rightGain > wide.leftGain);
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, wide.leftGain);
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.0f, wide.rightGain);
}

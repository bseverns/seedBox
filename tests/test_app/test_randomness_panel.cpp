#include <unity.h>

#include <vector>

#include "app/AppState.h"
#include "hal/Board.h"
#include "hal/hal_io.h"

namespace {

constexpr uint32_t kDeterministicMasterSeed = 0xDEADBEEFu;
constexpr int kSimulationTicks = 192;

void runTicks(AppState& app, int count) {
  for (int i = 0; i < count; ++i) {
    app.tick();
  }
}

std::vector<Seed> captureDeterministicSeeds(uint32_t masterSeed) {
  hal::nativeBoardReset();
  auto& board = hal::nativeBoard();
  AppState app(board);
  app.initSim();

  auto& panel = app.randomnessPanel();
  panel.entropy = 0.0f;
  panel.mutationRate = 0.0f;
  panel.repeatBias = 0.0f;
  panel.resetBehavior = AppState::RandomnessPanel::ResetBehavior::Hard;

  app.reseed(masterSeed);
  runTicks(app, kSimulationTicks);
  return app.seeds();
}

void assertGranularEquals(const Seed::GranularParams& baseline, const Seed::GranularParams& candidate) {
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, baseline.grainSizeMs, candidate.grainSizeMs);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, baseline.sprayMs, candidate.sprayMs);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, baseline.transpose, candidate.transpose);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, baseline.windowSkew, candidate.windowSkew);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, baseline.stereoSpread, candidate.stereoSpread);
  TEST_ASSERT_EQUAL_UINT8(baseline.source, candidate.source);
  TEST_ASSERT_EQUAL_UINT8(baseline.sdSlot, candidate.sdSlot);
}

void assertResonatorEquals(const Seed::ResonatorParams& baseline, const Seed::ResonatorParams& candidate) {
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, baseline.exciteMs, candidate.exciteMs);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, baseline.damping, candidate.damping);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, baseline.brightness, candidate.brightness);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, baseline.feedback, candidate.feedback);
  TEST_ASSERT_EQUAL_UINT8(baseline.mode, candidate.mode);
  TEST_ASSERT_EQUAL_UINT8(baseline.bank, candidate.bank);
}

void assertSeedsMatch(const std::vector<Seed>& baseline, const std::vector<Seed>& candidate) {
  TEST_ASSERT_EQUAL_INT(static_cast<int>(baseline.size()), static_cast<int>(candidate.size()));
  const std::size_t limit = baseline.size();
  for (std::size_t i = 0; i < limit; ++i) {
    const Seed& expected = baseline[i];
    const Seed& actual = candidate[i];
    TEST_ASSERT_EQUAL_UINT32(expected.id, actual.id);
    TEST_ASSERT_EQUAL_UINT32(expected.prng, actual.prng);
    TEST_ASSERT_EQUAL_UINT32(expected.lineage, actual.lineage);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(expected.source), static_cast<uint8_t>(actual.source));
    TEST_ASSERT_EQUAL_UINT8(expected.engine, actual.engine);
    TEST_ASSERT_EQUAL_UINT8(expected.sampleIdx, actual.sampleIdx);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, expected.pitch, actual.pitch);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, expected.envA, actual.envA);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, expected.envD, actual.envD);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, expected.envS, actual.envS);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, expected.envR, actual.envR);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, expected.density, actual.density);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, expected.probability, actual.probability);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, expected.jitterMs, actual.jitterMs);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, expected.tone, actual.tone);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, expected.spread, actual.spread);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, expected.mutateAmt, actual.mutateAmt);
    assertGranularEquals(expected.granular, actual.granular);
    assertResonatorEquals(expected.resonator, actual.resonator);
  }
}

}  // namespace

void test_randomness_panel_entropy_zero_is_deterministic() {
  const auto baseline = captureDeterministicSeeds(kDeterministicMasterSeed);
  const auto repeat = captureDeterministicSeeds(kDeterministicMasterSeed);
  assertSeedsMatch(baseline, repeat);
}

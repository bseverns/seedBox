#include <unity.h>

#include <algorithm>

#include "Seed.h"
#include "app/AppState.h"
#include "hal/Board.h"

namespace {

constexpr uint32_t kDeterministicMasterSeed = 0xFEEDB0ADu;

}  // namespace

void test_learn_frame_reports_generator_state() {
  hal::nativeBoardReset();
  auto& board = hal::nativeBoard();
  AppState app(board);
  app.initSim();
  app.reseed(kDeterministicMasterSeed);

  AppState::LearnFrame frame{};
  app.captureLearnFrame(frame);

  const auto& seeds = app.seeds();
  const std::size_t focusIndex = static_cast<std::size_t>(app.focusSeed());
  TEST_ASSERT_FALSE(seeds.empty());
  TEST_ASSERT_EQUAL_UINT32(seeds[focusIndex].id, frame.generator.focusSeedId);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, seeds[focusIndex].density, frame.generator.density);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, seeds[focusIndex].probability, frame.generator.probability);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, seeds[focusIndex].mutateAmt, frame.generator.focusMutateAmt);
  TEST_ASSERT_EQUAL_UINT32(
      static_cast<uint32_t>(std::count_if(seeds.begin(), seeds.end(), [](const Seed& seed) {
        return seed.mutateAmt > 0.0f;
      })),
      frame.generator.mutationCount);
  TEST_ASSERT_EQUAL(frame.generator.primeMode, app.seedPrimeMode());
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, app.currentTapTempoBpm(), frame.generator.tapTempoBpm);

  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, frame.audio.combinedRms);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, frame.audio.combinedPeak);
}

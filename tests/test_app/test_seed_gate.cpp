#include <vector>
#include <unity.h>

#include "app/AppState.h"
#include "hal/Board.h"

namespace {

std::vector<float> makeBuffer(std::size_t frames, float value) {
  return std::vector<float>(frames, value);
}

void pumpTicks(AppState& app, int count) {
  for (int i = 0; i < count; ++i) {
    app.tick();
  }
}

}  // namespace

void test_gate_reseed_respects_division_and_locks() {
  hal::nativeBoardReset();
  auto& board = hal::nativeBoard();
  AppState app(board);
  app.initSim();

  app.setInputGateDivisionFromHost(AppState::GateDivision::kOneOverFour);
  const uint32_t firstMaster = app.masterSeed();

  const auto hot = makeBuffer(96, 0.05f);
  const auto cold = makeBuffer(96, 0.0f);

  app.setDryInputFromHost(hot.data(), nullptr, hot.size());

  pumpTicks(app, 5);
  TEST_ASSERT_EQUAL_UINT32(firstMaster, app.masterSeed());

  pumpTicks(app, 2);
  const uint32_t reseeded = app.masterSeed();
  TEST_ASSERT_NOT_EQUAL(firstMaster, reseeded);

  app.setDryInputFromHost(cold.data(), nullptr, cold.size());
  pumpTicks(app, 4);
  app.seedPageToggleLock(app.focusSeed());
  app.setDryInputFromHost(hot.data(), nullptr, hot.size());
  pumpTicks(app, 8);
  TEST_ASSERT_EQUAL_UINT32(reseeded, app.masterSeed());
}

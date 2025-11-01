#include <unity.h>

#include <cstring>

#include "app/AppState.h"
#include "hal/Board.h"

namespace {

void runTicks(AppState& app, int count) {
  for (int i = 0; i < count; ++i) {
    app.tick();
  }
}

bool statusContains(const AppState::DisplaySnapshot& snap, const char* needle) {
  return std::strstr(snap.status, needle) != nullptr;
}

void setUp() {}
void tearDown() {}

void test_initial_mode_home() {
  hal::nativeBoardReset();
  auto& board = hal::nativeBoard();
  AppState app(board);
  app.initSim();
  app.tick();
  TEST_ASSERT_EQUAL(AppState::Mode::HOME, app.mode());
  AppState::DisplaySnapshot snap{};
  app.captureDisplaySnapshot(snap);
  TEST_ASSERT_TRUE(statusContains(snap, "HOME"));
}

void test_seed_button_transitions_to_seeds() {
  hal::nativeBoardReset();
  auto& board = hal::nativeBoard();
  AppState app(board);
  app.initSim();
  hal::nativeBoardFeed("btn seed down");
  hal::nativeBoardFeed("wait 30ms");
  hal::nativeBoardFeed("btn seed up");
  runTicks(app, 24);
  TEST_ASSERT_EQUAL(AppState::Mode::SEEDS, app.mode());
  AppState::DisplaySnapshot snap{};
  app.captureDisplaySnapshot(snap);
  TEST_ASSERT_TRUE(statusContains(snap, "SEEDS"));
}

void test_shift_long_press_returns_home() {
  hal::nativeBoardReset();
  auto& board = hal::nativeBoard();
  AppState app(board);
  app.initSim();
  hal::nativeBoardFeed("btn seed down");
  hal::nativeBoardFeed("wait 30ms");
  hal::nativeBoardFeed("btn seed up");
  runTicks(app, 24);
  TEST_ASSERT_EQUAL(AppState::Mode::SEEDS, app.mode());

  hal::nativeBoardFeed("btn shift down");
  hal::nativeBoardFeed("wait 600ms");
  hal::nativeBoardFeed("btn shift up");
  runTicks(app, 80);
  TEST_ASSERT_EQUAL(AppState::Mode::HOME, app.mode());
}

void test_double_tap_moves_to_settings() {
  hal::nativeBoardReset();
  auto& board = hal::nativeBoard();
  AppState app(board);
  app.initSim();
  hal::nativeBoardFeed("btn tap down");
  hal::nativeBoardFeed("wait 40ms");
  hal::nativeBoardFeed("btn tap up");
  hal::nativeBoardFeed("wait 150ms");
  hal::nativeBoardFeed("btn tap down");
  hal::nativeBoardFeed("wait 40ms");
  hal::nativeBoardFeed("btn tap up");
  runTicks(app, 60);
  TEST_ASSERT_EQUAL(AppState::Mode::SETTINGS, app.mode());
  AppState::DisplaySnapshot snap{};
  app.captureDisplaySnapshot(snap);
  TEST_ASSERT_TRUE(statusContains(snap, "SET"));
}

void test_chord_shift_alt_seed_enters_perf() {
  hal::nativeBoardReset();
  auto& board = hal::nativeBoard();
  AppState app(board);
  app.initSim();
  // Enter settings first via double tap.
  hal::nativeBoardFeed("btn tap down");
  hal::nativeBoardFeed("wait 40ms");
  hal::nativeBoardFeed("btn tap up");
  hal::nativeBoardFeed("wait 150ms");
  hal::nativeBoardFeed("btn tap down");
  hal::nativeBoardFeed("wait 40ms");
  hal::nativeBoardFeed("btn tap up");
  runTicks(app, 60);
  TEST_ASSERT_EQUAL(AppState::Mode::SETTINGS, app.mode());

  hal::nativeBoardFeed("btn shift down");
  hal::nativeBoardFeed("btn alt down");
  hal::nativeBoardFeed("wait 40ms");
  hal::nativeBoardFeed("btn alt up");
  hal::nativeBoardFeed("btn shift up");
  runTicks(app, 20);
  TEST_ASSERT_EQUAL(AppState::Mode::PERF, app.mode());
}

void test_shift_hold_rotate_moves_focus() {
  hal::nativeBoardReset();
  auto& board = hal::nativeBoard();
  AppState app(board);
  app.initSim();
  const uint8_t initialFocus = app.focusSeed();
  TEST_ASSERT_FALSE(app.seeds().empty());
  hal::nativeBoardFeed("btn shift down");
  hal::nativeBoardFeed("wait 30ms");
  hal::nativeBoardFeed("enc seed 2");
  hal::nativeBoardFeed("wait 30ms");
  hal::nativeBoardFeed("btn shift up");
  runTicks(app, 20);
  TEST_ASSERT_EQUAL(static_cast<uint8_t>((initialFocus + 2) % app.seeds().size()), app.focusSeed());
}

}  // namespace

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_initial_mode_home);
  RUN_TEST(test_seed_button_transitions_to_seeds);
  RUN_TEST(test_shift_long_press_returns_home);
  RUN_TEST(test_double_tap_moves_to_settings);
  RUN_TEST(test_chord_shift_alt_seed_enters_perf);
  RUN_TEST(test_shift_hold_rotate_moves_focus);
  return UNITY_END();
}


#include <unity.h>

#include <cstring>
#include <algorithm>
#include <string>
#include <vector>

#include "app/AppState.h"
#include "hal/Board.h"
#include "hal/hal_io.h"

namespace {

constexpr hal::io::PinNumber kReseedPin = 2;
constexpr hal::io::PinNumber kLockPin = 3;

struct PanelClock {
  std::uint32_t micros{0};
  void reset() { micros = 0; }
  std::uint32_t advance(std::uint32_t delta) {
    micros += delta;
    return micros;
  }
  std::uint32_t now() const { return micros; }
};

void runTicks(AppState& app, int count) {
  for (int i = 0; i < count; ++i) {
    app.tick();
  }
}

bool statusContains(const AppState::DisplaySnapshot& snap, const char* needle) {
  return std::strstr(snap.status, needle) != nullptr;
}

void tapButton(AppState& app, const char* name, int holdMs = 40, int settleTicks = 24) {
  std::string press = std::string("btn ") + name + " down";
  std::string wait = std::string("wait ") + std::to_string(holdMs) + "ms";
  std::string release = std::string("btn ") + name + " up";
  hal::nativeBoardFeed(press.c_str());
  hal::nativeBoardFeed(wait.c_str());
  hal::nativeBoardFeed(release.c_str());
  runTicks(app, settleTicks);
}

void pressLockButton(AppState& app, PanelClock& clock, bool longPress) {
  clock.advance(1000);
  hal::io::mockSetDigitalInput(kLockPin, true, clock.now());
  app.tick();
  const int idleTicks = longPress ? 48 : 8;
  for (int i = 0; i < idleTicks; ++i) {
    app.tick();
  }
  clock.advance(longPress ? 650000 : 120000);
  hal::io::mockSetDigitalInput(kLockPin, false, clock.now());
  app.tick();
  runTicks(app, 6);
}

void pressStorageButton(AppState& app, PanelClock& clock, bool longPress) {
  clock.advance(2000);
  hal::io::mockSetDigitalInput(kReseedPin, true, clock.now());
  app.tick();
  const int holdFrames = longPress ? 80 : 12;
  for (int i = 0; i < holdFrames; ++i) {
    app.tick();
  }
  clock.advance(longPress ? 700000 : 90000);
  hal::io::mockSetDigitalInput(kReseedPin, false, clock.now());
  app.tick();
  runTicks(app, 10);
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

void test_scripted_front_panel_walkthrough() {
  hal::nativeBoardReset();
  auto& board = hal::nativeBoard();
  AppState app(board);
  app.initSim();
  PanelClock clock{};

  // Walk the four encoder buttons to tour the page stack.
  tapButton(app, "density");
  TEST_ASSERT_EQUAL(AppState::Mode::ENGINE, app.mode());
  tapButton(app, "tone");
  TEST_ASSERT_EQUAL(AppState::Mode::PERF, app.mode());
  tapButton(app, "fx");
  TEST_ASSERT_EQUAL(AppState::Mode::UTIL, app.mode());

  // Double tap the transport to reach settings and chord back into performance.
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

  // Long-press shift to return home.
  hal::nativeBoardFeed("btn shift down");
  hal::nativeBoardFeed("wait 600ms");
  hal::nativeBoardFeed("btn shift up");
  runTicks(app, 80);
  TEST_ASSERT_EQUAL(AppState::Mode::HOME, app.mode());

  // Reseed via a long hold on the Seed encoder button.
  const uint32_t beforeSeed = app.masterSeed();
  const auto beforeSeeds = app.seeds();
  hal::nativeBoardFeed("btn seed down");
  hal::nativeBoardFeed("wait 520ms");
  hal::nativeBoardFeed("btn seed up");
  runTicks(app, 96);
  TEST_ASSERT_NOT_EQUAL(beforeSeed, app.masterSeed());
  TEST_ASSERT_FALSE(beforeSeeds.empty());

  // Short presses of the dedicated lock key toggle the focus seed.
  TEST_ASSERT_FALSE(app.isSeedLocked(app.focusSeed()));
  pressLockButton(app, clock, false);
  TEST_ASSERT_TRUE(app.isSeedLocked(app.focusSeed()));
  pressLockButton(app, clock, false);
  TEST_ASSERT_FALSE(app.isSeedLocked(app.focusSeed()));

  // Long presses flip the global lock latch.
  pressLockButton(app, clock, true);
  TEST_ASSERT_TRUE(app.isGlobalSeedLocked());
  pressLockButton(app, clock, true);
  TEST_ASSERT_FALSE(app.isGlobalSeedLocked());

  // Park on the storage page so the reseed button becomes save/recall.
  app.setPage(AppState::Page::kStorage);
  const auto savedSeeds = app.seeds();
  pressStorageButton(app, clock, true);
  TEST_ASSERT_EQUAL_STRING("default", app.activePresetSlot().c_str());

  // Mutate the table, then short-press to recall the saved preset with crossfade.
  app.reseed(app.masterSeed() + 37u);
  pressStorageButton(app, clock, false);
  runTicks(app, static_cast<int>(AppState::kPresetCrossfadeTicks + 8));
  TEST_ASSERT_EQUAL(savedSeeds.size(), app.seeds().size());
  if (!savedSeeds.empty()) {
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, savedSeeds.front().pitch, app.seeds().front().pitch);
  }

  AppState::DisplaySnapshot snap{};
  app.captureDisplaySnapshot(snap);
  TEST_ASSERT_TRUE(statusContains(snap, "HOME"));
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
  RUN_TEST(test_scripted_front_panel_walkthrough);
  return UNITY_END();
}


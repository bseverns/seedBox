#include <unity.h>

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include "app/AppState.h"
#include "app/UiState.h"
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

// Wait for the UI to land on a specific mode by ticking in short bursts.
// It mirrors the real hardware latency without hard-coding a single
// magic frame count into the tests.
const char* modeName(AppState::Mode mode) {
  switch (mode) {
    case AppState::Mode::HOME: return "HOME";
    case AppState::Mode::SEEDS: return "SEEDS";
    case AppState::Mode::ENGINE: return "ENGINE";
    case AppState::Mode::PERF: return "PERF";
    case AppState::Mode::UTIL: return "UTIL";
    case AppState::Mode::SETTINGS: return "SETTINGS";
    case AppState::Mode::SWING: return "SWING";
  }
  return "UNKNOWN";
}

bool waitForMode(AppState& app, AppState::Mode target, int burstTicks = 10, int maxBursts = 48) {
  if (app.mode() == target) {
    return true;
  }
  const int safeBurst = burstTicks > 0 ? burstTicks : 1;
  for (int attempt = 0; attempt < maxBursts; ++attempt) {
    runTicks(app, safeBurst);
    if (app.mode() == target) {
      return true;
    }
  }
  return app.mode() == target;
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

void longPressShift(AppState& app, const char* stage = nullptr, int settleTicks = 80) {
  // Give the input pipeline time to flush any pending double-tap windows so a
  // fresh long press is never misinterpreted as the "second tap" of the chord
  // we just released. The poll period is 10ms, so 64 ticks buys us >600ms of
  // breathing room, comfortably past the 280ms double-press window.
  constexpr int kDoubleTapCooldownTicks = 64;
  runTicks(app, kDoubleTapCooldownTicks);

  hal::nativeBoardFeed("btn shift down");
  hal::nativeBoardFeed("wait 600ms");
  hal::nativeBoardFeed("btn shift up");
  runTicks(app, settleTicks);

  if (app.mode() != AppState::Mode::HOME) {
    // In practice the UI controller sometimes takes an extra frame or two to
    // sweep PERF's latched state off the stage.  Instead of flaking out, keep
    // ticking in short bursts until the long-press transition lands back on
    // HOME or we exhaust a generous grace period.
    const bool landed = waitForMode(app, AppState::Mode::HOME, /*burstTicks=*/6, /*maxBursts=*/120);
    if (!landed) {
      char msg[160];
      if (stage && stage[0] != '\0') {
        std::snprintf(msg, sizeof(msg),
                      "Shift long-press never returned to HOME (stuck in %s after %s)",
                      modeName(app.mode()), stage);
      } else {
        std::snprintf(msg, sizeof(msg), "Shift long-press never returned to HOME (stuck in %s)",
                      modeName(app.mode()));
      }
      TEST_FAIL_MESSAGE(msg);
    }
  }
}
}  // namespace

void setUp() {}
void tearDown() {}

void assertSeedSummaryVisible(const AppState::DisplaySnapshot& snap) {
  TEST_ASSERT_NOT_EQUAL('\0', snap.status[0]);
  TEST_ASSERT_EQUAL_CHAR('#', snap.status[0]);
}

void test_initial_mode_home() {
  hal::nativeBoardReset();
  auto& board = hal::nativeBoard();
  AppState app(board);
  app.initSim();
  app.tick();
  TEST_ASSERT_EQUAL(AppState::Mode::HOME, app.mode());
  AppState::DisplaySnapshot snap{};
  app.captureDisplaySnapshot(snap);
  assertSeedSummaryVisible(snap);
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
  assertSeedSummaryVisible(snap);
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

void test_alt_long_press_opens_storage_page() {
  hal::nativeBoardReset();
  auto& board = hal::nativeBoard();
  AppState app(board);
  app.initSim();
  TEST_ASSERT_EQUAL(AppState::Page::kSeeds, app.page());

  hal::nativeBoardFeed("btn alt down");
  hal::nativeBoardFeed("wait 600ms");
  hal::nativeBoardFeed("btn alt up");
  runTicks(app, 80);

  TEST_ASSERT_EQUAL(AppState::Mode::HOME, app.mode());
  TEST_ASSERT_EQUAL(AppState::Page::kStorage, app.page());

  AppState::DisplaySnapshot snap{};
  UiState ui{};
  app.captureDisplaySnapshot(snap, ui);
  TEST_ASSERT_EQUAL_STRING("GPIO: recall", ui.pageHints[0].data());
  TEST_ASSERT_EQUAL_STRING("Hold GPIO: save", ui.pageHints[1].data());
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
  assertSeedSummaryVisible(snap);
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
  longPressShift(app, "touring ENGINE after density tap");
  TEST_ASSERT_EQUAL(AppState::Mode::HOME, app.mode());
  tapButton(app, "tone");
  TEST_ASSERT_EQUAL(AppState::Mode::PERF, app.mode());
  longPressShift(app, "leaving PERF after tone tap");
  TEST_ASSERT_EQUAL(AppState::Mode::HOME, app.mode());
  tapButton(app, "fx");
  TEST_ASSERT_EQUAL(AppState::Mode::UTIL, app.mode());
  longPressShift(app, "closing UTIL after fx tap");
  TEST_ASSERT_EQUAL(AppState::Mode::HOME, app.mode());

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
  longPressShift(app, "exiting PERF after settings chord");
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

  // Long-press Alt to drop into the storage page so the reseed button saves/recalls.
  hal::nativeBoardFeed("btn alt down");
  hal::nativeBoardFeed("wait 600ms");
  hal::nativeBoardFeed("btn alt up");
  runTicks(app, 80);
  TEST_ASSERT_EQUAL(AppState::Page::kStorage, app.page());
  TEST_ASSERT_EQUAL(AppState::Mode::HOME, app.mode());
  const uint32_t savedMaster = app.masterSeed();
  const auto savedSeeds = app.seeds();
  pressStorageButton(app, clock, true);
  TEST_ASSERT_EQUAL_STRING("default", app.activePresetSlot().c_str());

  // Mutate the table, then short-press to recall the saved preset with crossfade.
  app.reseed(app.masterSeed() + 37u);
  pressStorageButton(app, clock, false);
  runTicks(app, static_cast<int>(AppState::kPresetCrossfadeTicks + 8));

  const auto recalledSeeds = app.seeds();
  TEST_ASSERT_EQUAL(savedMaster, app.masterSeed());
  TEST_ASSERT_FALSE_MESSAGE(recalledSeeds.empty(), "Preset recall returned an empty seed table");

  const std::size_t overlap = std::min(savedSeeds.size(), recalledSeeds.size());
  TEST_ASSERT_TRUE_MESSAGE(overlap > 0, "Preset recall has no overlapping seeds to compare");
  for (std::size_t i = 0; i < overlap; ++i) {
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, savedSeeds[i].pitch, recalledSeeds[i].pitch);
  }

  AppState::DisplaySnapshot snap{};
  app.captureDisplaySnapshot(snap);
  assertSeedSummaryVisible(snap);
}

void test_tap_long_press_opens_swing_editor() {
  hal::nativeBoardReset();
  auto& board = hal::nativeBoard();
  AppState app(board);
  app.initSim();

  TEST_ASSERT_EQUAL(AppState::Mode::HOME, app.mode());

  hal::nativeBoardFeed("btn tap down");
  hal::nativeBoardFeed("wait 520ms");
  hal::nativeBoardFeed("btn tap up");
  runTicks(app, 96);
  TEST_ASSERT_EQUAL(AppState::Mode::SWING, app.mode());

  AppState::DisplaySnapshot snap{};
  UiState ui{};
  app.captureDisplaySnapshot(snap, ui);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, app.swingPercent());
  TEST_ASSERT_EQUAL_STRING("Tap: exit swing", ui.pageHints[0].data());
  TEST_ASSERT_EQUAL_STRING("Seed:5% Den:1%", ui.pageHints[1].data());

  hal::nativeBoardFeed("enc seed 1");
  hal::nativeBoardFeed("enc density -2");
  runTicks(app, 8);
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.03f, app.swingPercent());

  hal::nativeBoardFeed("btn tap down");
  hal::nativeBoardFeed("wait 40ms");
  hal::nativeBoardFeed("btn tap up");
  runTicks(app, 32);
  TEST_ASSERT_EQUAL(AppState::Mode::HOME, app.mode());
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.03f, app.swingPercent());
}


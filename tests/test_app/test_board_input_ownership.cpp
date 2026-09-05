#include <unity.h>
#include <chrono>
#include <thread>

#include "app/AppState.h"
#include "hal/Board.h"
#include "hal/hal_io.h"

void test_raw_gpio_cannot_reseed_or_lock() {
  hal::nativeBoardReset();
  // Seed an unrelated GPIO configuration before constructing the app. The
  // constructor must neither replace it nor subscribe to its edges.
  const hal::io::DigitalConfig output{12, false, false};
  hal::io::init(&output, 1);
  hal::io::writeDigital(12, true);
  AppState app;
  app.initSim();
  TEST_ASSERT_TRUE(hal::io::readDigital(12));
  const auto seed = app.masterSeed();
  for (const auto pin : {2, 3}) {
    hal::io::mockSetDigitalInput(pin, true, 1000);
    hal::io::poll();
    app.tick();
    hal::io::mockSetDigitalInput(pin, false, 701000);
    hal::io::poll();
    app.serviceHostMaintenance();
  }
  TEST_ASSERT_EQUAL_UINT32(seed, app.masterSeed());
  TEST_ASSERT_FALSE(app.isGlobalSeedLocked());
  for (unsigned i = 0; i < app.seeds().size(); ++i) {
    TEST_ASSERT_FALSE(app.isSeedLocked(i));
  }
}

void test_board_seed_press_and_density_turn_do_not_reseed_or_lock() {
  hal::nativeBoardReset();
  AppState app;
  app.initSim();
  const auto seed = app.masterSeed();
  hal::nativeBoardSetButton(hal::Board::ButtonID::EncoderSeedBank, true);
  app.tick();
  TEST_ASSERT_EQUAL(AppState::Mode::SEEDS, app.mode());
  hal::nativeBoardSetButton(hal::Board::ButtonID::EncoderSeedBank, false);
  app.tick();
  hal::nativeBoardFeed("enc density +1");
  for (int i = 0; i < 40; ++i) app.tick();
  TEST_ASSERT_EQUAL_UINT32(seed, app.masterSeed());
  TEST_ASSERT_FALSE(app.isSeedLocked(app.focusSeed()));
  TEST_ASSERT_FALSE(app.isGlobalSeedLocked());

  hal::nativeBoardSetButton(hal::Board::ButtonID::EncoderSeedBank, true);
  app.serviceHostMaintenance();
  hal::nativeBoardFastForwardMicros(500000);
  app.serviceHostMaintenance();
  TEST_ASSERT_NOT_EQUAL(seed, app.masterSeed());
  const auto reseeded = app.masterSeed();
  hal::nativeBoardFastForwardMicros(500000);
  app.serviceHostMaintenance();
  hal::nativeBoardSetButton(hal::Board::ButtonID::EncoderSeedBank, false);
  app.serviceHostMaintenance();
  TEST_ASSERT_EQUAL_UINT32(reseeded, app.masterSeed());
}

void test_native_board_realtime_clock_preserves_scripted_default() {
  hal::nativeBoardReset();
  auto& board = hal::nativeBoard();
  board.poll();
  TEST_ASSERT_EQUAL_UINT64(10000, board.nowMicros());
  hal::nativeBoardUseRealtimeClock(true);
  const auto before = board.nowMicros();
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  board.poll();
  TEST_ASSERT_TRUE(board.nowMicros() - before >= 20000);
  // Reset restores deterministic time for the next scripted rehearsal.
  hal::nativeBoardReset();
  board.poll();
  TEST_ASSERT_EQUAL_UINT64(10000, board.nowMicros());
}

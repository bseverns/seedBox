#include <unity.h>
#include <string>

#include "app/InputEvents.h"
#include "hal/PanelControls.h"

namespace {
void sample(hal::Board& board, InputEvents& input) {
  board.poll();
  input.update();
}
void assertOnlyButton(hal::Board& board, hal::Board::ButtonID id, bool down) {
  for (std::size_t i = 0; i < hal::panel::buttonCount; ++i) {
    const auto other = static_cast<hal::Board::ButtonID>(i);
    TEST_ASSERT_EQUAL(down && other == id, board.sampleButton(other).pressed);
  }
}
void exerciseButton(const char* token, hal::Board::ButtonID id) {
  hal::nativeBoardReset();
  auto& board = hal::nativeBoard();
  InputEvents input(board);
  hal::nativeBoardFeed(std::string("btn ") + token + " down");
  sample(board, input);
  assertOnlyButton(board, id, true);
  TEST_ASSERT_EQUAL_UINT(1, input.events().size());
  TEST_ASSERT_EQUAL(InputEvents::Type::ButtonPress, input.events()[0].type);
  TEST_ASSERT_EQUAL(id, input.events()[0].primaryButton);
  // Holding produces one long event and no cross-talk or repeated presses.
  hal::nativeBoardFeed("wait 500ms");
  unsigned longEvents = 0;
  for (int tick = 0; tick < 60; ++tick) {
    sample(board, input);
    for (const auto& event : input.events()) {
      TEST_ASSERT_EQUAL(InputEvents::Type::ButtonLongPress, event.type);
      TEST_ASSERT_EQUAL(id, event.primaryButton);
      ++longEvents;
    }
  }
  TEST_ASSERT_EQUAL_UINT(1, longEvents);
  hal::nativeBoardFeed(std::string("btn ") + token + " up");
  sample(board, input);
  assertOnlyButton(board, id, false);
  TEST_ASSERT_EQUAL_UINT(1, input.events().size());
  TEST_ASSERT_EQUAL(InputEvents::Type::ButtonRelease, input.events()[0].type);
  TEST_ASSERT_EQUAL(id, input.events()[0].primaryButton);
  TEST_ASSERT_TRUE(input.events()[0].heldUs >= 500000);
  sample(board, input);
  TEST_ASSERT_TRUE(input.events().empty());
}
}  // namespace

void test_every_panel_button_independently() {
  for (const auto& encoder : hal::panel::encoders) exerciseButton(encoder.token, encoder.button_id);
  for (const auto& button : hal::panel::buttons) exerciseButton(button.token, button.id);
}

void test_every_panel_encoder_independently() {
  for (std::size_t i = 0; i < hal::panel::encoders.size(); ++i) {
    hal::nativeBoardReset();
    auto& board = hal::nativeBoard();
    InputEvents input(board);
    const auto& encoder = hal::panel::encoders[i];
    for (const int delta : {3, -2}) {
      hal::nativeBoardFeed(std::string("enc ") + encoder.token + " " + std::to_string(delta));
      sample(board, input);
      TEST_ASSERT_EQUAL_UINT(1, input.events().size());
      const auto& event = input.events()[0];
      TEST_ASSERT_EQUAL(InputEvents::Type::EncoderTurn, event.type);
      TEST_ASSERT_EQUAL(encoder.id, event.encoder);
      TEST_ASSERT_EQUAL_INT(delta, event.encoderDelta);
      assertOnlyButton(board, encoder.button_id, false);
      sample(board, input);
      TEST_ASSERT_TRUE(input.events().empty());  // Delta was consumed once.
    }
    // The integrated switch must identify the same encoder during a hold-turn.
    hal::nativeBoardFeed(std::string("btn ") + encoder.token + " down");
    sample(board, input);
    hal::nativeBoardFeed(std::string("enc ") + encoder.token + " 1");
    sample(board, input);
    TEST_ASSERT_EQUAL_UINT(1, input.events().size());
    const auto& event = input.events()[0];
    TEST_ASSERT_EQUAL(InputEvents::Type::EncoderHoldTurn, event.type);
    TEST_ASSERT_EQUAL(encoder.id, event.encoder);
    TEST_ASSERT_EQUAL_UINT(1, event.buttons.size());
    TEST_ASSERT_EQUAL(encoder.button_id, event.buttons[0]);
  }
  hal::nativeBoardReset();
}

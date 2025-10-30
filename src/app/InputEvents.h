#pragma once

//
// Input event pipeline
// --------------------
// This layer chews on the raw board state (debounced buttons + encoder deltas)
// and spits out semantic actions: single presses, long presses, double taps,
// button chords, and hold+rotate gestures.  The controller pages can then talk
// about intent ("Shift + Alt" vs. "pin 32 low") which keeps both the firmware
// and the classroom demos human-friendly.

#include <array>
#include <cstdint>
#include <vector>

#include "hal/Board.h"

class InputEvents {
public:
  enum class Type : std::uint8_t {
    ButtonPress,
    ButtonLongPress,
    ButtonDoublePress,
    ButtonChord,
    EncoderTurn,
    EncoderHoldTurn,
  };

  struct Event {
    Type type{Type::ButtonPress};
    hal::Board::ButtonID primaryButton{hal::Board::ButtonID::TapTempo};
    std::vector<hal::Board::ButtonID> buttons{};
    hal::Board::EncoderID encoder{hal::Board::EncoderID::SeedBank};
    int32_t encoderDelta{0};
    std::uint64_t timestampUs{0};
  };

  explicit InputEvents(hal::Board& board);

  void update();
  const std::vector<Event>& events() const { return events_; }
  void clear();

  bool buttonDown(hal::Board::ButtonID id) const;

private:
  using ButtonMask = std::uint32_t;

  struct ButtonState {
    bool down{false};
    bool longSent{false};
    bool awaitingSecond{false};
    std::uint64_t lastChange{0};
    std::uint64_t lastRelease{0};
  };

  ButtonMask maskFor(const std::vector<hal::Board::ButtonID>& buttons) const;
  ButtonMask maskFor(hal::Board::ButtonID button) const;

  void handleButton(hal::Board::ButtonID id, const hal::Board::ButtonSample& sample, std::uint64_t now);
  void handleEncoder(hal::Board::EncoderID id, int32_t delta, std::uint64_t now);
  void flushPendingPresses(std::uint64_t now);
  void emit(Event&& evt);

  hal::Board& board_;
  std::array<ButtonState, 7> button_states_{};
  ButtonMask held_mask_{0};
  std::vector<Event> events_{};
  std::vector<std::pair<hal::Board::ButtonID, std::uint64_t>> pending_presses_{};
  std::vector<ButtonMask> active_chords_{};

  const std::uint64_t long_press_threshold_us_{450000};
  const std::uint64_t double_press_window_us_{280000};
  const std::uint64_t chord_window_us_{100000};
};


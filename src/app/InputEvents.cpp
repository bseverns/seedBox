#include "app/InputEvents.h"

#include <algorithm>

namespace {

constexpr std::uint32_t buttonBit(hal::Board::ButtonID id) {
  return 1u << static_cast<std::uint32_t>(id);
}

}  // namespace

InputEvents::InputEvents(hal::Board& board) : board_(board) {}

void InputEvents::update() {
  events_.clear();

  const std::uint64_t now = board_.nowMicros();
  for (std::size_t i = 0; i < button_states_.size(); ++i) {
    const auto id = static_cast<hal::Board::ButtonID>(i);
    handleButton(id, board_.sampleButton(id), now);
  }

  flushPendingPresses(now);

  for (std::size_t i = 0; i < 4; ++i) {
    const auto enc = static_cast<hal::Board::EncoderID>(i);
    const int32_t delta = board_.consumeEncoderDelta(enc);
    if (delta != 0) {
      handleEncoder(enc, delta, now);
    }
  }
}

void InputEvents::clear() {
  events_.clear();
  pending_presses_.clear();
}

bool InputEvents::buttonDown(hal::Board::ButtonID id) const {
  const auto idx = static_cast<std::size_t>(id);
  if (idx >= button_states_.size()) {
    return false;
  }
  return button_states_[idx].down;
}

InputEvents::ButtonMask InputEvents::maskFor(const std::vector<hal::Board::ButtonID>& buttons) const {
  ButtonMask mask = 0;
  for (const auto& id : buttons) {
    mask |= maskFor(id);
  }
  return mask;
}

InputEvents::ButtonMask InputEvents::maskFor(hal::Board::ButtonID button) const {
  return buttonBit(button);
}

void InputEvents::handleButton(hal::Board::ButtonID id, const hal::Board::ButtonSample& sample,
                               std::uint64_t now) {
  const std::size_t idx = static_cast<std::size_t>(id);
  ButtonState& state = button_states_[idx];
  const bool pressed = sample.pressed;

  if (pressed != state.down) {
    state.down = pressed;
    state.lastChange = now;

    if (pressed) {
      held_mask_ |= buttonBit(id);
      state.longSent = false;
      if (state.awaitingSecond && (now - state.lastRelease) <= double_press_window_us_) {
        Event evt{};
        evt.type = Type::ButtonDoublePress;
        evt.primaryButton = id;
        evt.buttons = {id};
        evt.timestampUs = now;
        emit(std::move(evt));
        state.awaitingSecond = false;
        state.longSent = true;
      } else {
        pending_presses_.emplace_back(id, now);
      }

      // Chord detection: gather all buttons currently held and fire once per unique mask.
      if (held_mask_ != 0) {
        std::vector<hal::Board::ButtonID> chordButtons;
        for (std::size_t n = 0; n < button_states_.size(); ++n) {
          if (button_states_[n].down) {
            chordButtons.push_back(static_cast<hal::Board::ButtonID>(n));
          }
        }
        if (chordButtons.size() >= 2) {
          std::sort(chordButtons.begin(), chordButtons.end(),
                    [](auto a, auto b) { return static_cast<int>(a) < static_cast<int>(b); });
          const ButtonMask mask = maskFor(chordButtons);
          const bool alreadyActive = std::find(active_chords_.begin(), active_chords_.end(), mask) != active_chords_.end();
          const bool withinWindow = (now - sample.timestamp_us) <= chord_window_us_;
          if (!alreadyActive && withinWindow) {
            Event evt{};
            evt.type = Type::ButtonChord;
            evt.buttons = chordButtons;
            evt.primaryButton = id;
            evt.timestampUs = now;
            emit(std::move(evt));
            active_chords_.push_back(mask);
          }
        }
      }
    } else {
      held_mask_ &= ~buttonBit(id);
      state.lastRelease = now;
      if (!state.longSent) {
        state.awaitingSecond = true;
      } else {
        state.awaitingSecond = false;
      }

      // Tear down any chords that included this button.
      active_chords_.erase(std::remove_if(active_chords_.begin(), active_chords_.end(),
                                          [&](ButtonMask mask) { return (mask & buttonBit(id)) != 0; }),
                           active_chords_.end());
    }
    return;
  }

  if (pressed) {
    if (!state.longSent && (now - state.lastChange) >= long_press_threshold_us_) {
      Event evt{};
      evt.type = Type::ButtonLongPress;
      evt.primaryButton = id;
      evt.buttons = {id};
      evt.timestampUs = now;
      emit(std::move(evt));
      state.longSent = true;
      state.awaitingSecond = false;
    }
  }
}

void InputEvents::handleEncoder(hal::Board::EncoderID id, int32_t delta, std::uint64_t now) {
  Event evt{};
  evt.encoder = id;
  evt.encoderDelta = delta;
  evt.timestampUs = now;
  if (held_mask_ != 0) {
    evt.type = Type::EncoderHoldTurn;
    for (std::size_t i = 0; i < button_states_.size(); ++i) {
      if (button_states_[i].down) {
        evt.buttons.push_back(static_cast<hal::Board::ButtonID>(i));
      }
    }
  } else {
    evt.type = Type::EncoderTurn;
  }
  emit(std::move(evt));
}

void InputEvents::flushPendingPresses(std::uint64_t now) {
  if (pending_presses_.empty()) {
    return;
  }

  auto it = pending_presses_.begin();
  while (it != pending_presses_.end()) {
    const hal::Board::ButtonID id = it->first;
    ButtonState& state = button_states_[static_cast<std::size_t>(id)];
    if (state.awaitingSecond && (now - state.lastRelease) <= double_press_window_us_) {
      ++it;
      continue;
    }
    if (state.awaitingSecond && (now - state.lastRelease) > double_press_window_us_) {
      state.awaitingSecond = false;
    }
    if (!state.awaitingSecond && !state.longSent) {
      Event evt{};
      evt.type = Type::ButtonPress;
      evt.primaryButton = id;
      evt.buttons = {id};
      evt.timestampUs = now;
      emit(std::move(evt));
    }
    it = pending_presses_.erase(it);
  }
}

void InputEvents::emit(Event&& evt) { events_.push_back(std::move(evt)); }


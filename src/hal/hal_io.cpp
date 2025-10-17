// GPIO helper for both the Teensy target and the simulator.  We keep it noisy so
// students can see how polling, callbacks, and test doubles line up.
#include "hal_io.h"

#include <algorithm>
#include <cstdint>
#include <vector>

#ifndef SEEDBOX_HW
#include <queue>
#endif

#ifdef SEEDBOX_HW
#include <Arduino.h>
#endif

namespace hal {
namespace io {

namespace {
struct PinState {
  PinNumber pin;
  bool is_input;
  bool pullup;
  bool last_level;
};

std::vector<PinState> g_pins;
DigitalCallback g_callback = nullptr;
void *g_user_data = nullptr;

#ifndef SEEDBOX_HW
struct PendingEvent {
  PinNumber pin;
  bool level;
  std::uint32_t timestamp;
};

std::queue<PendingEvent> g_events;
#endif

PinState *findPin(PinNumber pin) {
  auto it = std::find_if(g_pins.begin(), g_pins.end(),
                         [pin](const PinState &state) { return state.pin == pin; });
  if (it == g_pins.end()) {
    return nullptr;
  }
  return &(*it);
}

}  // namespace

void init(const DigitalConfig *configs, std::size_t count) {
  // Stash pin metadata so later `poll` calls know which GPIOs to watch.
  g_pins.clear();
  g_pins.reserve(count);
  for (std::size_t i = 0; i < count; ++i) {
    const DigitalConfig &cfg = configs[i];
    PinState state{cfg.pin, cfg.input, cfg.pullup, false};
#ifdef SEEDBOX_HW
    if (state.is_input) {
      pinMode(state.pin, state.pullup ? INPUT_PULLUP : INPUT);
      state.last_level = digitalRead(state.pin) == HIGH;
    } else {
      pinMode(state.pin, OUTPUT);
      digitalWrite(state.pin, LOW);
      state.last_level = false;
    }
#else
    state.last_level = false;
#endif
    g_pins.push_back(state);
  }
}

void setDigitalCallback(DigitalCallback callback, void *user_data) {
  g_callback = callback;
  g_user_data = user_data;
}

void poll() {
#ifdef SEEDBOX_HW
  // On hardware we sample each configured input and fire the callback whenever
  // the level flips.  Polling once per main loop keeps things deterministic and
  // easy to explain.
  const std::uint32_t now = micros();
  for (auto &state : g_pins) {
    if (!state.is_input) {
      continue;
    }
    const bool level = digitalRead(state.pin) == HIGH;
    if (level != state.last_level) {
      state.last_level = level;
      if (g_callback) {
        g_callback(state.pin, level, now, g_user_data);
      }
    }
  }
#else
  // The simulator pushes synthetic edge events into a queue.  Tests can feed it
  // timestamps to mimic bouncing buttons or frantic encoder spins.
  while (!g_events.empty()) {
    const PendingEvent evt = g_events.front();
    g_events.pop();
    if (PinState *state = findPin(evt.pin)) {
      state->last_level = evt.level;
    }
    if (g_callback) {
      g_callback(evt.pin, evt.level, evt.timestamp, g_user_data);
    }
  }
#endif
}

void writeDigital(PinNumber pin, bool level) {
  if (PinState *state = findPin(pin)) {
    state->last_level = level;
  }
#ifdef SEEDBOX_HW
  // Mirror the cached state out to the actual GPIO line.
  digitalWrite(pin, level ? HIGH : LOW);
#endif
}

bool readDigital(PinNumber pin) {
#ifdef SEEDBOX_HW
  return digitalRead(pin) == HIGH;
#else
  if (PinState *state = findPin(pin)) {
    // Simulator path simply echoes whatever the tests last wrote.
    return state->last_level;
  }
  return false;
#endif
}

#ifndef SEEDBOX_HW
void mockSetDigitalInput(PinNumber pin, bool level, std::uint32_t timestamp_us) {
  if (PinState *state = findPin(pin)) {
    if (state->last_level == level) {
      return;
    }
  }
  // Queue the event so `poll()` processes it in order, just like a hardware
  // edge would arrive asynchronously.
  g_events.push(PendingEvent{pin, level, timestamp_us});
}
#endif

}  // namespace io
}  // namespace hal


#pragma once

//
// HAL shim for the Serial7 UART MIDI port.
// ----------------------------------------
// The MN42 controller talks MIDI over the Type-A mini TRS pair.  This facade
// hides the mess of running-status parsing and lets the rest of the firmware
// treat it like yet another callback-driven transport source.

#include <cstdint>

namespace hal {
namespace midi {
namespace serial7 {

struct Handlers {
  void (*clock)(void* user_data);
  void (*start)(void* user_data);
  void (*stop)(void* user_data);
  void (*control_change)(std::uint8_t channel, std::uint8_t controller,
                         std::uint8_t value, void* user_data);
};

// Boot the UART and install callbacks.
void begin(const Handlers& handlers, void* user_data = nullptr);
// Call this from loop() to slurp bytes and emit events.
void poll();

}  // namespace serial7
}  // namespace midi
}  // namespace hal


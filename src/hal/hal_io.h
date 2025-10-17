#pragma once

//
// HAL GPIO facade.
// -----------------
// These helpers let the high-level firmware pretend that both the simulator and
// the Teensy expose the same interface.  Keeping the API tiny (init/poll/read)
// makes it easy to mock in tests and easy to reason about in class.

#include <cstddef>
#include <cstdint>

namespace hal {
namespace io {

using PinNumber = std::uint8_t;

struct DigitalConfig {
  PinNumber pin;
  bool input;
  bool pullup;
};

using DigitalCallback = void (*)(PinNumber pin, bool level, std::uint32_t timestamp_us,
                                 void *user_data);

// Configure the pins once at boot.
void init(const DigitalConfig *configs, std::size_t count);
// Register a callback that fires whenever any watched input toggles.
void setDigitalCallback(DigitalCallback callback, void *user_data = nullptr);
// Pump this from the main loop to detect edges.
void poll();

void writeDigital(PinNumber pin, bool level);
bool readDigital(PinNumber pin);

#ifndef SEEDBOX_HW
// Simulator helper: pretend an input pin changed state at a specific time.
void mockSetDigitalInput(PinNumber pin, bool level, std::uint32_t timestamp_us = 0);
#endif  // SEEDBOX_HW

}  // namespace io
}  // namespace hal


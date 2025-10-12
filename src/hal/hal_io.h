#pragma once

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

void init(const DigitalConfig *configs, std::size_t count);
void setDigitalCallback(DigitalCallback callback, void *user_data = nullptr);
void poll();

void writeDigital(PinNumber pin, bool level);
bool readDigital(PinNumber pin);

#ifndef SEEDBOX_HW
void mockSetDigitalInput(PinNumber pin, bool level, std::uint32_t timestamp_us = 0);
#endif  // SEEDBOX_HW

}  // namespace io
}  // namespace hal


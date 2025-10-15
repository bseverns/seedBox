#pragma once

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

void begin(const Handlers& handlers, void* user_data = nullptr);
void poll();

}  // namespace serial7
}  // namespace midi
}  // namespace hal


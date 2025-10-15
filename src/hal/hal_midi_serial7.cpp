#include "hal/hal_midi_serial7.h"

#include <cstdint>

#ifdef SEEDBOX_HW
#include <Arduino.h>
#include "HardwareConfig.h"
#endif

namespace hal {
namespace midi {
namespace serial7 {

namespace {
#ifdef SEEDBOX_HW
Handlers g_handlers{};
void* g_user_data = nullptr;
std::uint8_t g_running_status = 0;
std::uint8_t g_expected_bytes = 0;
std::uint8_t g_data_bytes[2] = {0, 0};
std::uint8_t g_data_count = 0;

inline void dispatchClock() {
  if (g_handlers.clock) {
    g_handlers.clock(g_user_data);
  }
}

inline void dispatchStart() {
  if (g_handlers.start) {
    g_handlers.start(g_user_data);
  }
}

inline void dispatchStop() {
  if (g_handlers.stop) {
    g_handlers.stop(g_user_data);
  }
}

inline void dispatchControlChange(std::uint8_t channel, std::uint8_t controller,
                                  std::uint8_t value) {
  if (g_handlers.control_change) {
    g_handlers.control_change(channel, controller, value, g_user_data);
  }
}

std::uint8_t expectedDataBytes(std::uint8_t status) {
  const std::uint8_t high = status & 0xF0u;
  if (status >= 0xF0u) {
    switch (status) {
      case 0xF1:  // MTC Quarter Frame
      case 0xF3:  // Song Select
        return 1;
      case 0xF2:  // Song Position Pointer
        return 2;
      default:
        return 0;
    }
  }
  if (high == 0xC0u || high == 0xD0u) {
    return 1;
  }
  return 2;
}

void resetParser() {
  g_running_status = 0;
  g_expected_bytes = 0;
  g_data_count = 0;
  g_data_bytes[0] = 0;
  g_data_bytes[1] = 0;
}

void handleStatus(std::uint8_t status) {
  g_running_status = status;
  g_expected_bytes = expectedDataBytes(status);
  g_data_count = 0;
  g_data_bytes[0] = 0;
  g_data_bytes[1] = 0;
  if (g_expected_bytes == 0 && status < 0xF0u) {
    // Channel message with zero-length payload is bogus; drop running status.
    resetParser();
  }
}

void handleDataByte(std::uint8_t byte) {
  if (g_expected_bytes == 0) {
    return;
  }
  if (g_data_count < sizeof(g_data_bytes)) {
    g_data_bytes[g_data_count] = byte;
  }
  ++g_data_count;
  if (g_data_count < g_expected_bytes) {
    return;
  }

  const std::uint8_t status_high = g_running_status & 0xF0u;
  const std::uint8_t channel = g_running_status & 0x0Fu;
  if (status_high == 0xB0u && g_expected_bytes == 2) {
    dispatchControlChange(channel, g_data_bytes[0], g_data_bytes[1]);
  }

  if (g_running_status >= 0xF0u) {
    resetParser();
  } else {
    g_data_count = 0;
  }
}
#endif  // SEEDBOX_HW
}  // namespace

void begin(const Handlers& handlers, void* user_data) {
#ifdef SEEDBOX_HW
  g_handlers = handlers;
  g_user_data = user_data;
  resetParser();
  Serial7.setRX(HardwareConfig::kMidiTrsTypeAInRxPin);
  Serial7.setTX(HardwareConfig::kMidiTrsTypeAOutTxPin);
  Serial7.begin(31250);
#else
  (void)handlers;
  (void)user_data;
#endif
}

void poll() {
#ifdef SEEDBOX_HW
  while (Serial7.available() > 0) {
    const int raw = Serial7.read();
    if (raw < 0) {
      break;
    }
    const std::uint8_t byte = static_cast<std::uint8_t>(raw & 0xFF);

    if (byte >= 0xF8u) {
      switch (byte) {
        case 0xF8u:
          dispatchClock();
          break;
        case 0xFAu:
          dispatchStart();
          break;
        case 0xFCu:
          dispatchStop();
          break;
        default:
          break;
      }
      continue;
    }

    if ((byte & 0x80u) != 0) {
      handleStatus(byte);
      continue;
    }

    if (g_running_status == 0) {
      continue;
    }

    handleDataByte(byte);
  }
#endif
}

}  // namespace serial7
}  // namespace midi
}  // namespace hal


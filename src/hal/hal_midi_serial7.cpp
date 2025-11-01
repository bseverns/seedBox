// Serial7 MIDI bridge.  We unroll the running-status state machine in C++ so we
// can explain exactly how 1980s MIDI bytes become clean callbacks in 2024.
#include "hal/hal_midi_serial7.h"

#include <cstdint>

#if SEEDBOX_HW
#include <Arduino.h>
#include "HardwareConfig.h"
#endif

namespace hal {
namespace midi {
namespace serial7 {

namespace {
#if SEEDBOX_HW
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
  // Every MIDI status byte implies how many payload bytes follow.  We reproduce
  // that truth table here so the parser can stay stateless and debuggable.
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
    // Only CC messages matter for now.  Note-on/off plumbing can join later
    // without rewriting the parser.
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
#if SEEDBOX_HW
  // Hang onto the callbacks and prime Serial7 at the canonical 31.25 kbaud.
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
#if SEEDBOX_HW
  // Byte-by-byte state machine lifted straight from the MIDI spec.  We watch for
  // realtime messages (0xF8+) first since they can interleave anywhere, then
  // feed the running-status parser with the rest.
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

void sendClock() {
#if SEEDBOX_HW
  Serial7.write(0xF8u);
#endif
}

void sendStart() {
#if SEEDBOX_HW
  Serial7.write(0xFAu);
#endif
}

void sendStop() {
#if SEEDBOX_HW
  Serial7.write(0xFCu);
#endif
}

void sendControlChange(std::uint8_t channel, std::uint8_t controller, std::uint8_t value) {
#if SEEDBOX_HW
  const std::uint8_t status = static_cast<std::uint8_t>(0xB0u | (channel & 0x0Fu));
  Serial7.write(status);
  Serial7.write(controller & 0x7Fu);
  Serial7.write(value & 0x7Fu);
#else
  (void)channel;
  (void)controller;
  (void)value;
#endif
}

void sendNoteOn(std::uint8_t channel, std::uint8_t note, std::uint8_t velocity) {
#if SEEDBOX_HW
  const std::uint8_t status = static_cast<std::uint8_t>(0x90u | (channel & 0x0Fu));
  Serial7.write(status);
  Serial7.write(note & 0x7Fu);
  Serial7.write(velocity & 0x7Fu);
#else
  (void)channel;
  (void)note;
  (void)velocity;
#endif
}

void sendNoteOff(std::uint8_t channel, std::uint8_t note, std::uint8_t velocity) {
#if SEEDBOX_HW
  const std::uint8_t status = static_cast<std::uint8_t>(0x80u | (channel & 0x0Fu));
  Serial7.write(status);
  Serial7.write(note & 0x7Fu);
  Serial7.write(velocity & 0x7Fu);
#else
  (void)channel;
  (void)note;
  (void)velocity;
#endif
}

void sendAllNotesOff(std::uint8_t channel) {
#if SEEDBOX_HW
  sendControlChange(channel, 123u, 0u);
#else
  (void)channel;
#endif
}

}  // namespace serial7
}  // namespace midi
}  // namespace hal


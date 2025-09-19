#pragma once
#include <stdint.h>
#include <stddef.h>

namespace HardwareConfig {
static constexpr uint8_t kDisplayI2CAddress = 0x3C;
static constexpr uint8_t kDisplaySclPin = 19; // Teensy 4.0 default SCL
static constexpr uint8_t kDisplaySdaPin = 18; // Teensy 4.0 default SDA
static constexpr uint8_t kDisplayResetPin = 255; // not wired (SSD1306 auto reset)

struct EncoderPins {
  uint8_t pinA;
  uint8_t pinB;
  uint8_t switchPin;
  const char* label;
};

static constexpr EncoderPins kEncoders[] = {
  {0, 1, 2,   "Seed/Bank"},
  {3, 4, 5,   "Density/Prob"},
  {6, 7, 8,   "Tone/Tilt"},
  {9, 10, 11, "FX/Mutate"},
};

static constexpr size_t kEncoderCount = sizeof(kEncoders) / sizeof(kEncoders[0]);

static constexpr uint8_t kTapTempoPin = 12;
static constexpr uint8_t kShiftButtonPin = 13;
static constexpr uint8_t kAltSeedButtonPin = 14;

static constexpr uint8_t kExpression1Pin = 15; // analog CV 0-3.3V
static constexpr uint8_t kExpression2Pin = 16; // analog CV 0-3.3V
}

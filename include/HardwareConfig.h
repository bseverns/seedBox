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

// Encoder pins intentionally dodge the I2S + SPI busses so the SGTL5000 audio
// shield can sit directly on the Teensy headers. Keep the DIN MIDI port on its
// own serial RX (see kMidiDinInRxPin below) so clock/control can arrive from an
// opto-isolated jack without fighting USB.
static constexpr EncoderPins kEncoders[] = {
  {0, 1, 2,   "Seed/Bank"},
  {3, 4, 5,   "Density/Prob"},
  {24, 26, 27, "Tone/Tilt"},
  {28, 29, 30, "FX/Mutate"},
};

static constexpr size_t kEncoderCount = sizeof(kEncoders) / sizeof(kEncoders[0]);

// Buttons moved to the high-number analog-capable pads. Those pads are free on
// the audio shield and easy to ribbon over to a control panel.
static constexpr uint8_t kTapTempoPin = 31;
static constexpr uint8_t kShiftButtonPin = 32;
static constexpr uint8_t kAltSeedButtonPin = 33;

static constexpr uint8_t kExpression1Pin = 15; // analog CV 0-3.3V
static constexpr uint8_t kExpression2Pin = 16; // analog CV 0-3.3V

// External DIN MIDI input (5-pin jack through an opto-isolator). We park it on
// Serial6 RX so Serial1 stays free for debugging or future MIDI thru.
static constexpr uint8_t kMidiDinInRxPin = 25; // Teensy 4.0 Serial6 RX

struct AudioShieldPins {
  uint8_t mclkPin;   // master clock into SGTL5000
  uint8_t bclkPin;   // bit clock (I2S)
  uint8_t lrclkPin;  // word select (I2S)
  uint8_t txPin;     // Teensy -> codec audio data
  uint8_t rxPin;     // codec -> Teensy audio data
  uint8_t sdCsPin;   // SD card chip select (optional but common)
};

// Reserved SGTL5000/Audio shield pins. RX is the interesting one here: it feeds
// external audio straight into the seed engine so we can grain/slice incoming
// material instead of only playing back RAM/SD content.
static constexpr AudioShieldPins kAudioShield = {
  23, // MCLK
  21, // BCLK
  20, // LRCLK
  7,  // TX (Teensy -> codec)
  8,  // RX (codec -> Teensy, becomes "external audio in")
  10  // SD card CS (wired on the PJRC audio shield)
};
}

#pragma once

//
// Board interface
// ----------------
// The physical SeedBox surface — four encoders with integrated switches plus
// three transport/utility buttons — is abstracted behind this tiny interface so
// both the Teensy firmware and the native simulator speak the same control
// dialect.  The goal is pedagogical: students can read this header, see which
// buttons map to which pins, and then write tests or UI logic without touching
// a single hardware register.
//
// Hardware map cheat-sheet (mirrors docs/builder_bootstrap.md):
//   * EncoderSeedBank   — pins 0 (A) / 1 (B), push switch on pin 2.
//   * EncoderDensity    — pins 3 (A) / 4 (B), push switch on pin 5.
//   * EncoderToneTilt   — pins 24 (A) / 26 (B), push switch on pin 27.
//   * EncoderFxMutate   — pins 6 (A) / 9 (B), push switch on pin 30.
//   * TapTempo button   — pin 31 (active-low).
//   * Shift button      — pin 32 (active-low).
//   * AltSeed button    — pin 33 (active-low).
//
// Every button is active-low and debounced in the platform-specific
// implementation.  Encoders report signed deltas so higher layers can reason in
// “ticks” rather than raw quadrature edges.  `millis()` / `micros()` expose the
// board’s notion of time (Teensy hardware counters on device, deterministic
// simulated clocks in the native build).

#include <cstdint>
#include <string>

namespace hal {

class Board {
public:
  enum class ButtonID : std::uint8_t {
    EncoderSeedBank = 0,
    EncoderDensity,
    EncoderToneTilt,
    EncoderFxMutate,
    TapTempo,
    Shift,
    AltSeed,
  };

  enum class EncoderID : std::uint8_t {
    SeedBank = 0,
    Density,
    ToneTilt,
    FxMutate,
  };

  struct ButtonSample {
    bool pressed = false;
    std::uint64_t timestamp_us = 0;
  };

  virtual ~Board() = default;

  // Pump any pending GPIO state into the board cache.  Teensy builds call into
  // Bounce + hal::io while the native shim consumes scripted events.
  virtual void poll() = 0;

  // Return the most recent debounced level for the requested control.
  virtual ButtonSample sampleButton(ButtonID id) const = 0;

  // Pop the accumulated rotary delta (in detents) since the last call.
  virtual int32_t consumeEncoderDelta(EncoderID id) = 0;

  // Some lesson plans wire an external tap-tempo pedal into the TAP jack.  This
  // helper mirrors that state so tempo logic can treat it differently from the
  // front-panel button if desired.  (On the stock rig this is the same as
  // ButtonID::TapTempo.)
  virtual bool tapTempoActive() const = 0;

  virtual std::uint32_t nowMillis() const = 0;
  virtual std::uint64_t nowMicros() const = 0;
};

// Platform helpers ---------------------------------------------------------

// `board()` hands back the singleton for whichever target we are building.
// Teensy builds return the Bounce-backed hardware instance, while the native
// build returns the scripted simulator shim.
Board& board();

#if !SEEDBOX_HW
// Native test harness utilities.  `nativeBoard()` exposes the concrete type so
// unit tests can inject scripted button/encoder events and time jumps without
// leaking implementation details to production code.
Board& nativeBoard();
void nativeBoardFeed(const std::string& line);
void nativeBoardReset();
void nativeBoardFastForwardMicros(std::uint64_t delta);
#endif

}  // namespace hal


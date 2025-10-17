#pragma once

#include <stdint.h>

// Shared constants that describe how the SeedBox firmware speaks to the MN42
// controller.  Keeping them in one place means the docs, IO layer, and tests
// can reach for the same truth table without sprinkling magic numbers
// everywhere.
namespace seedbox::interop::mn42 {

// MN42 ships its CC traffic on channel 1 by default.  We keep the constant
// around so host-side tools can track future changes.  The firmware normalizes
// everything to a zero-based channel number so USB (1-based) and TRS (0-based)
// sources march in lockstep.
constexpr uint8_t kDefaultChannel = 0;

// Helper that translates the 1-16 channel numbering the Teensy USB stack uses
// into the zero-based representation shared by the rest of the app.  Channel 0
// is technically reserved in the MIDI spec, but usbMIDI defensively returns it
// for malformed packets, so clamp here instead of underflowing.
constexpr uint8_t NormalizeUsbChannel(uint8_t raw_channel) {
  return raw_channel == 0 ? 0 : static_cast<uint8_t>(raw_channel - 1);
}

// Control change numbers mirrored from the MN42 firmware expectations.
// The instrument exposes a tiny handshake and a few "mode" controls; anything
// beyond these numbers belongs in a higher-level learn table.
namespace cc {
constexpr uint8_t kHandshake = 14;      // Single-byte hello / ack CC.
constexpr uint8_t kMode = 15;           // Bit field describing active features.
constexpr uint8_t kSeedMorph = 16;      // 0-127 blend between stored seeds.
constexpr uint8_t kTransportGate = 17;  // Velocity-style latch for start/stop.
}  // namespace cc

// Mode bit field that both devices read/write using cc::kMode.  The constants
// describe intent instead of values so bit-twiddling stays obvious.
namespace mode {
constexpr uint8_t kFollowExternalClock = 0x01;  // When set, MN42 drives tempo.
constexpr uint8_t kExposeDebugMeters = 0x02;    // SeedBox streams raw levels.
constexpr uint8_t kArpAccent = 0x04;            // Enable MN42 accent lane.
constexpr uint8_t kLatchTransport = 0x08;       // Ignore momentary toggles.
}  // namespace mode

// Handshake flow lives on cc::kHandshake and a tiny SysEx ping.  The CC values
// act as human-readable checkpoints during setup or diagnostics.
namespace handshake {
constexpr uint8_t kHello = 0x01;        // MN42 fires this after boot.
constexpr uint8_t kAck = 0x11;          // SeedBox replies once routing is ready.
constexpr uint8_t kKeepAlive = 0x7F;    // Periodic pulse proving we are alive.

// SysEx identity payload.  MN42 uses the non-commercial ID 0x7D and sends a
// three-byte signature so we can reject stray controllers.
constexpr uint8_t kManufacturerId = 0x7D;
constexpr uint8_t kProductMajor = 0x4D;  // 'M'
constexpr uint8_t kProductMinor = 0x4E;  // 'N'
constexpr uint8_t kProductRevision = 0x42;  // 'B' for Box bridge.
}  // namespace handshake

}  // namespace seedbox::interop::mn42

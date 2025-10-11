#pragma once

#include <cstdint>

namespace seedbox::interop::mn42 {

// Shared control-change IDs between MN42 and SeedBox. Keep this table in sync
// with docs/interop_mn42.md.
namespace cc {
constexpr std::uint8_t kEngineCycle = 20;
constexpr std::uint8_t kDensity = 21;
constexpr std::uint8_t kProbability = 22;
constexpr std::uint8_t kTone = 23;
constexpr std::uint8_t kFx = 24;
constexpr std::uint8_t kStatusPing = 100;
}  // namespace cc

// Engine IDs exchanged during handshake.
namespace engine {
constexpr std::uint8_t kSampler = 0;
constexpr std::uint8_t kGranular = 1;
constexpr std::uint8_t kResonator = 2;
}  // namespace engine

// Mode bits for future expansion.
namespace mode {
constexpr std::uint8_t kQuiet = 0x01;      // Respect QUIET_MODE (no IO spam).
constexpr std::uint8_t kGolden = 0x02;     // Request golden-audio capture.
constexpr std::uint8_t kPerformance = 0x04;  // Full interop, no guard rails.
}  // namespace mode

}  // namespace seedbox::interop::mn42

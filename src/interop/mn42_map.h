#pragma once

#include <cstdint>

namespace seedbox::interop::mn42 {

constexpr std::uint8_t kCcReseed = 14;
constexpr std::uint8_t kCcDensityNudge = 21;
constexpr std::uint8_t kCcEngineCycle = 51;
constexpr std::uint8_t kCcHeadlessToggle = 90;

constexpr std::uint8_t kSysExVendor = 0x77;
constexpr std::uint8_t kSysExHandshake = 0x01;

inline bool matches_handshake(std::uint8_t msb, std::uint8_t lsb) {
  return msb == kSysExVendor && lsb == kSysExHandshake;
}

}  // namespace seedbox::interop::mn42

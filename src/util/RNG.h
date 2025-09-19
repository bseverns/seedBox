#pragma once
#include <cstdint>

namespace RNG {
// xorshift32 for determinism
inline uint32_t xorshift(uint32_t& state) {
  uint32_t x = state ? state : 2463534242u;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  state = x;
  return x;
}
inline float uniform01(uint32_t& state) {
  return (xorshift(state) >> 8) * (1.0f / 16777216.0f); // 24-bit mantissa
}
} // namespace RNG

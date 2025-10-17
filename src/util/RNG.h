#pragma once
#include <cstdint>

namespace RNG {

//
// xorshift
// --------
// Tiny deterministic pseudo-random number generator that shows up everywhere in
// the firmware.  We keep it header-only so students can inline step through it
// with a debugger and actually *see* the bit-twiddling.  The routine accepts a
// mutable `state` reference so the caller can stash the generator alongside a
// Seed and replay the exact same random walk later.
inline uint32_t xorshift(uint32_t& state) {
  // Guard against the zero state (which would otherwise get stuck there
  // forever) by seeding with a known-good value.  The constant is the same one
  // used in George Marsaglia's original paper.
  uint32_t x = state ? state : 2463534242u;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  state = x;
  return x;
}

// Convert the 32-bit xorshift result into a float in the 0..1 range.  We lop off
// the lowest eight bits so the mantissa lines up with the 24-bit precision of a
// single-precision float — a nice talking point when explaining how floating
// point works under the hood.
inline float uniform01(uint32_t& state) {
  return (xorshift(state) >> 8) * (1.0f / 16777216.0f); // 24-bit mantissa
}

// Symmetric variant centred around zero.  Great for jitter and spray knobs where
// a negative offset needs to be just as likely as a positive one.
inline float uniformSigned(uint32_t& state) {
  return (uniform01(state) * 2.0f) - 1.0f;
}

} // namespace RNG

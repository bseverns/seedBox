#pragma once
#include <stdint.h>

namespace Units {
#ifdef SEEDBOX_HW
  // In hardware, you'd query audio sample rate; default Teensy Audio is 44117-ish.
  static constexpr float kSampleRate = 44100.0f;
#else
  static constexpr float kSampleRate = 48000.0f;
#endif

inline uint32_t msToSamples(float ms) {
  return static_cast<uint32_t>((ms * 0.001f) * kSampleRate);
}

// Simple monotonic sample counter for sim
#ifndef SEEDBOX_HW
inline uint32_t simNowSamples() {
  static uint32_t acc = 0;
  acc += 200; // ~200 samples per tick placeholder (~4 ms at 48k)
  return acc;
}
#else
inline uint32_t simNowSamples() { return 0; }
#endif

} // namespace Units

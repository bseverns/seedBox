#pragma once
#include <stdint.h>

namespace Units {

// Central stash for transport-related constants.  The sampler and scheduler both
// lean on this so the "what is a sample" question only has to be answered in one
// spot.  Hardware builds honour the Teensy Audio library's canonical rate while
// the simulator picks a friendlier 48 kHz number that keeps math clean for
// desktop tests.
#ifdef SEEDBOX_HW
  static constexpr float kSampleRate = 44100.0f;
#else
  static constexpr float kSampleRate = 48000.0f;
#endif

// Convert milliseconds into integer sample counts.  Putting the formula in a
// helper makes it easier to show students the unit conversion without having the
// same `(ms * 0.001f) * sampleRate` expression sprinkled through every engine.
inline uint32_t msToSamples(float ms) {
  return static_cast<uint32_t>((ms * 0.001f) * kSampleRate);
}

//
// Simulated transport clock.
// -------------------------
// On real hardware we ask the PatternScheduler for "now" by peeking at the
// Teensy audio callback.  The simulator does not have that luxury, so we keep a
// monotonic counter that advances by a fixed amount every tick.  200 samples at
// 48 kHz is roughly 4.16 ms — close enough to drive tests while remaining easy
// to reason about on a whiteboard.
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

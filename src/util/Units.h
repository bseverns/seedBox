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
// Teensy audio callback.  The simulator previously faked this with a fixed
// 200-sample step, but the scheduler now owns a BPM-aware cursor.  These helpers
// stick around for older tests that still want direct control over the mock
// clock.
#ifndef SEEDBOX_HW
  inline constexpr uint32_t kSimTickSamples = 200; // ~4 ms at 48k per scheduler tick

  namespace detail {
    inline uint32_t& simSampleCounter() {
      static uint32_t acc = 0;
      return acc;
    }
  }  // namespace detail

  inline uint32_t simNowSamples() {
    return detail::simSampleCounter();
  }

  inline uint32_t simAdvanceTickSamples() {
    uint32_t& acc = detail::simSampleCounter();
    acc += kSimTickSamples;
    return acc;
  }

  inline void simResetSamples(uint32_t value = 0) {
    detail::simSampleCounter() = value;
  }
#else
  inline uint32_t simNowSamples() { return 0; }
  inline uint32_t simAdvanceTickSamples() { return 0; }
  inline void simResetSamples(uint32_t = 0) {}
#endif

} // namespace Units

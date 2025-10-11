#pragma once

#include <cstdint>

namespace seedbox::hal {

struct AudioTiming {
  std::uint32_t sampleRate;
  std::uint16_t blockSize;
  constexpr float deadlineMs() const {
    return (blockSize == 0u || sampleRate == 0u)
               ? 0.0f
               : static_cast<float>(blockSize) * 1000.0f /
                     static_cast<float>(sampleRate);
  }
};

// Prepare the audio backend. On hardware we spin up the SGTL5000 codec and
// reserve memory blocks. In native builds this is a no-op so tests can link
// without Teensy headers.
void bootAudioBackend();

// Return the compile-time timing contract. Tests rely on this to verify that
// scheduling remains inside the 2.67 ms buffer window.
AudioTiming timing();

}  // namespace seedbox::hal

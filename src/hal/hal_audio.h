#pragma once

#include <cstddef>
#include <cstdint>

namespace seedbox::hal {

struct AudioSettings {
  uint16_t memory_blocks = 64;
  float output_volume = 0.6f;
  float sample_rate_hz = 48000.0f;
  std::size_t frames_per_buffer = 128;
};

struct AudioMetrics {
  float sample_rate_hz = 48000.0f;
  std::size_t frames_per_buffer = 128;
  std::uint64_t buffers_dispatched = 0;
};

// Initialise the underlying audio driver. Safe to call more than once; each
// call resets internal counters.
void init_audio(const AudioSettings& settings = AudioSettings{});

// Tear down the audio driver. No-op on native builds.
void shutdown_audio();

// Returns the metrics captured during the last initialisation / callback.
AudioMetrics audio_metrics();

#ifndef SEEDBOX_HW
// Native harnesses can manually tick the mock callback loop. The callback itself
// is owned by the engine; we just increment the buffer counter.
void mock_dispatch_buffer();
#endif

}  // namespace seedbox::hal

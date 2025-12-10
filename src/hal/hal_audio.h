#pragma once

//
// HAL audio facade.
// -----------------
// Presents a uniform callback-driven audio surface to both the simulator and
// the Teensy runtime.  Tests can feed fake buffers, hardware pumps the real
// I²S codec, and the higher layers never have to care which universe they're in.

#include <cstddef>
#include <cstdint>
#include "util/Annotations.h"

namespace hal {
namespace audio {

struct StereoBufferView {
  // Raw pointers into interleaved scratch buffers.  Higher-level code writes
  // into these and the HAL handles pushing them to hardware or test harnesses.
  float *left;
  float *right;
  std::size_t frames;
};

// The callback receives a writable stereo buffer and the user-provided context
// pointer.  Identical signature on both sim + hardware builds keeps life easy.
using Callback = void (*)(const StereoBufferView &buffer, void *user_data);

// Hook the callback and prep the backend.  Call this once at boot.
SEEDBOX_MAYBE_UNUSED void init(Callback callback, void *user_data = nullptr);
SEEDBOX_MAYBE_UNUSED void start();
SEEDBOX_MAYBE_UNUSED void stop();
SEEDBOX_MAYBE_UNUSED void shutdown();

SEEDBOX_MAYBE_UNUSED std::size_t framesPerBlock();
SEEDBOX_MAYBE_UNUSED float sampleRate();
SEEDBOX_MAYBE_UNUSED std::uint32_t sampleClock();

#if !SEEDBOX_HW
// Tell the sim/desktop path what the host's buffer sizing looks like. JUCE and
// other native audio drivers call this when they learn the device configuration
// so the engine prep code sees the real sample rate and block size.
SEEDBOX_MAYBE_UNUSED void configureHostStream(float sample_rate, std::size_t frames_per_block);
// Pump the audio callback using host-provided buffers. Drivers that already
// hand us float buffers (e.g. JUCE) can call this directly inside their audio
// callback instead of going through mockPump.
SEEDBOX_MAYBE_UNUSED void renderHostBuffer(float* left, float* right, std::size_t frames);
// Simulator hooks to control timing from unit tests.
SEEDBOX_MAYBE_UNUSED void mockSetSampleRate(float hz);
SEEDBOX_MAYBE_UNUSED void mockPump(std::size_t frames);
#endif  // SEEDBOX_HW

}  // namespace audio
}  // namespace hal


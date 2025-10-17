#pragma once

//
// HAL audio facade.
// -----------------
// Presents a uniform callback-driven audio surface to both the simulator and
// the Teensy runtime.  Tests can feed fake buffers, hardware pumps the real
// I²S codec, and the higher layers never have to care which universe they're in.

#include <cstddef>
#include <cstdint>

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
void init(Callback callback, void *user_data = nullptr);
void start();
void stop();
void shutdown();

std::size_t framesPerBlock();
float sampleRate();

#ifndef SEEDBOX_HW
// Simulator hooks to control timing from unit tests.
void mockSetSampleRate(float hz);
void mockPump(std::size_t frames);
#endif  // SEEDBOX_HW

}  // namespace audio
}  // namespace hal


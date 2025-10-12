#pragma once

#include <cstddef>
#include <cstdint>

namespace hal {
namespace audio {

struct StereoBufferView {
  float *left;
  float *right;
  std::size_t frames;
};

using Callback = void (*)(const StereoBufferView &buffer, void *user_data);

void init(Callback callback, void *user_data = nullptr);
void start();
void stop();
void shutdown();

std::size_t framesPerBlock();
float sampleRate();

#ifndef SEEDBOX_HW
void mockSetSampleRate(float hz);
void mockPump(std::size_t frames);
#endif  // SEEDBOX_HW

}  // namespace audio
}  // namespace hal


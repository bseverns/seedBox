#include "hal_audio.h"

#include <algorithm>
#include <array>
#include <vector>

#ifdef SEEDBOX_HW
#include <Arduino.h>
#include <AudioStream.h>
#endif

namespace hal {
namespace audio {

namespace {
Callback g_callback = nullptr;
void *g_user_data = nullptr;
bool g_running = false;
float g_sample_rate = 44100.0f;
std::size_t g_frames_per_block = 64;

#ifdef SEEDBOX_HW
class CallbackStream : public AudioStream {
 public:
  CallbackStream() : AudioStream(0, nullptr) {}

  void update() override {
    audio_block_t *left_block = allocate();
    audio_block_t *right_block = allocate();
    if (!left_block || !right_block) {
      if (left_block) {
        release(left_block);
      }
      if (right_block) {
        release(right_block);
      }
      return;
    }

    std::array<float, AUDIO_BLOCK_SAMPLES> left{};
    std::array<float, AUDIO_BLOCK_SAMPLES> right{};

    if (g_running && g_callback) {
      StereoBufferView view{left.data(), right.data(), left.size()};
      g_callback(view, g_user_data);
    } else {
      left.fill(0.0f);
      right.fill(0.0f);
    }

    for (std::size_t i = 0; i < left.size(); ++i) {
      const float l = std::clamp(left[i], -1.0f, 1.0f);
      const float r = std::clamp(right[i], -1.0f, 1.0f);
      left_block->data[i] = static_cast<int16_t>(l * 32767.0f);
      right_block->data[i] = static_cast<int16_t>(r * 32767.0f);
    }

    transmit(left_block, 0);
    transmit(right_block, 1);
    release(left_block);
    release(right_block);
  }
};

CallbackStream &streamInstance() {
  static CallbackStream stream;
  return stream;
}
#endif  // SEEDBOX_HW

}  // namespace

void init(Callback callback, void *user_data) {
  g_callback = callback;
  g_user_data = user_data;
  g_running = false;
#ifdef SEEDBOX_HW
  g_sample_rate = AUDIO_SAMPLE_RATE_EXACT;
  g_frames_per_block = AUDIO_BLOCK_SAMPLES;
  (void)streamInstance();
#else
  g_frames_per_block = 128;
#endif
}

void start() { g_running = true; }

void stop() { g_running = false; }

void shutdown() {
  stop();
  g_callback = nullptr;
  g_user_data = nullptr;
}

std::size_t framesPerBlock() { return g_frames_per_block; }

float sampleRate() { return g_sample_rate; }

#ifndef SEEDBOX_HW
namespace {
std::vector<float> &scratch(std::vector<float> &buffer, std::size_t frames) {
  if (buffer.size() != frames) {
    buffer.assign(frames, 0.0f);
  } else {
    std::fill(buffer.begin(), buffer.end(), 0.0f);
  }
  return buffer;
}
}  // namespace

void mockSetSampleRate(float hz) { g_sample_rate = hz; }

void mockPump(std::size_t frames) {
  if (!g_running || !g_callback || frames == 0) {
    return;
  }

  static std::vector<float> left;
  static std::vector<float> right;
  StereoBufferView view{scratch(left, frames).data(), scratch(right, frames).data(), frames};
  g_callback(view, g_user_data);
}
#endif  // SEEDBOX_HW

}  // namespace audio
}  // namespace hal


// Audio HAL implementation shared by sim + hardware builds.  The goal is to let
// the higher level code think in "frames" and "callbacks" without caring about
// the gritty Teensy plumbing.
#include "hal_audio.h"

#include <algorithm>
#include <array>
#include <atomic>
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
std::atomic<uint32_t> g_sample_clock{0};

#ifdef SEEDBOX_HW
class CallbackStream : public AudioStream {
 public:
  CallbackStream() : AudioStream(0, nullptr) {}

  void update() override {
    audio_block_t *left_block = allocate();
    audio_block_t *right_block = allocate();
    if (!left_block || !right_block) {
      // Failing to grab buffers is rare but possible when the AudioMemory pool
      // is exhausted.  We bail gracefully and retry on the next callback.
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
      // Convert the int16_t AudioStream buffers into floating point scratch so
      // our engines can stay sample-format agnostic.  Later we quantize back
      // down before handing the blocks to the codec.
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
    g_sample_clock.fetch_add(static_cast<uint32_t>(AUDIO_BLOCK_SAMPLES), std::memory_order_relaxed);
  }
};

CallbackStream &streamInstance() {
  static CallbackStream stream;
  return stream;
}
#endif  // SEEDBOX_HW

}  // namespace

void init(Callback callback, void *user_data) {
  // Remember the callback so both mock and hardware paths can yank buffers from
  // it later.
  g_callback = callback;
  g_user_data = user_data;
  g_running = false;
  g_sample_clock.store(0, std::memory_order_relaxed);
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

std::uint32_t sampleClock() { return static_cast<std::uint32_t>(g_sample_clock.load(std::memory_order_relaxed)); }

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

void mockSetSampleRate(float hz) {
  // Handy for tests that want to explore how tempo math behaves at different
  // sample rates without touching hardware globals.
  g_sample_rate = hz;
}

void mockPump(std::size_t frames) {
  if (!g_running || !g_callback || frames == 0) {
    return;
  }

  static std::vector<float> left;
  static std::vector<float> right;
  // Simulator: allocate two scratch buffers and hand them to the registered
  // callback.  This mirrors what the hardware path does with AudioStream blocks
  // so unit tests exercise the same logic.
  StereoBufferView view{scratch(left, frames).data(), scratch(right, frames).data(), frames};
  g_callback(view, g_user_data);
  g_sample_clock.fetch_add(static_cast<uint32_t>(frames), std::memory_order_relaxed);
}
#endif  // SEEDBOX_HW

}  // namespace audio
}  // namespace hal


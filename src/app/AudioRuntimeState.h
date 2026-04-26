#pragma once

#include <cstddef>
#include <cstdint>

class AudioRuntimeState {
 public:
  void resetHostState(bool hostAudioMode);
  void resetAudioCallbackCount();

  void incrementAudioCallbackCount();
  std::uint64_t audioCallbackCount() const { return audioCallbackCount_; }

  void setTestToneEnabled(bool enabled) { testToneEnabled_ = enabled; }
  bool testToneEnabled() const { return testToneEnabled_; }

  bool hostAudioMode() const { return hostAudioMode_; }

  void applyHostOutputSafety(float* left, float* right, std::size_t frames);
  void renderTestTone(float* left, float* right, std::size_t frames, float sampleRate);

 private:
  bool hostAudioMode_{false};
  float hostLimiterGain_{1.0f};
  bool testToneEnabled_{false};
  float testTonePhase_{0.0f};
  std::uint64_t audioCallbackCount_{0};
};

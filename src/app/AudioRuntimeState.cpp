#include "app/AudioRuntimeState.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr float kHostOutputTrim = 0.62f;
}

void AudioRuntimeState::resetHostState(bool hostAudioMode) {
  hostAudioMode_ = hostAudioMode;
  hostLimiterGain_ = 1.0f;
}

void AudioRuntimeState::resetAudioCallbackCount() { audioCallbackCount_ = 0; }

void AudioRuntimeState::incrementAudioCallbackCount() { ++audioCallbackCount_; }

void AudioRuntimeState::applyHostOutputSafety(float* left, float* right, std::size_t frames) {
  if (!left || !right || frames == 0) {
    return;
  }

  float peak = 0.0f;
  for (std::size_t i = 0; i < frames; ++i) {
    peak = std::max(peak, std::fabs(left[i]));
    peak = std::max(peak, std::fabs(right[i]));
  }

  constexpr float kTargetPeak = 0.72f;
  constexpr float kAttack = 0.24f;
  constexpr float kRelease = 0.025f;
  constexpr float kSoftClipDrive = 1.15f;
  constexpr float kSoftClipOut = 0.86f;

  const float desiredGain = (peak > kTargetPeak && peak > 0.0f) ? (kTargetPeak / peak) : 1.0f;
  const float slew = (desiredGain < hostLimiterGain_) ? kAttack : kRelease;
  hostLimiterGain_ += (desiredGain - hostLimiterGain_) * slew;
  hostLimiterGain_ = std::clamp(hostLimiterGain_, 0.12f, 1.0f);

  for (std::size_t i = 0; i < frames; ++i) {
    const float l = left[i] * kHostOutputTrim * hostLimiterGain_;
    const float r = right[i] * kHostOutputTrim * hostLimiterGain_;
    left[i] = std::tanh(l * kSoftClipDrive) * kSoftClipOut;
    right[i] = std::tanh(r * kSoftClipDrive) * kSoftClipOut;
  }
}

void AudioRuntimeState::renderTestTone(float* left, float* right, std::size_t frames, float sampleRate) {
  if (!left || !right || frames == 0) {
    return;
  }

  const float sr = sampleRate > 0.0f ? sampleRate : 48000.0f;
  const float frequency = 440.0f;
  const float gain = 0.08f;
  constexpr float kTwoPi = 6.2831853071795864769f;
  const float phaseStep = kTwoPi * frequency / sr;
  for (std::size_t i = 0; i < frames; ++i) {
    const float sample = std::sin(testTonePhase_) * gain;
    left[i] += sample;
    right[i] += sample;
    testTonePhase_ += phaseStep;
    if (testTonePhase_ >= kTwoPi) {
      testTonePhase_ -= kTwoPi;
    }
  }
}

#include "engine/ToyGenerator.h"

#include <algorithm>
#include <cmath>

#include "Seed.h"

namespace {
constexpr float kTwoPi = 6.2831853071795864769f;
constexpr float kBaseHz = 220.0f;
constexpr float kMaxAmplitude = 0.2f;

float semitoneToRatio(float semitones) {
  return std::pow(2.0f, semitones / 12.0f);
}
}

Engine::Type ToyGenerator::type() const noexcept { return Engine::Type::kToy; }

void ToyGenerator::prepare(const PrepareContext& ctx) {
  sampleRate_ = ctx.sampleRate > 0 ? static_cast<float>(ctx.sampleRate) : 48000.0f;
  voices_.fill(Voice{});
  nextVoice_ = 0;
  renderSample_ = 0;
}

void ToyGenerator::onTick(const TickContext& ctx) {
  (void)ctx;
}

void ToyGenerator::onParam(const ParamChange& change) {
  (void)change;
}

void ToyGenerator::onSeed(const SeedContext& ctx) {
  trigger(ctx.seed, ctx.whenSamples);
}

void ToyGenerator::renderAudio(const RenderContext& ctx) {
#if SEEDBOX_HW
  (void)ctx;
  return;
#else
  if (!ctx.left || !ctx.right || ctx.frames == 0) {
    return;
  }

  for (std::size_t i = 0; i < ctx.frames; ++i) {
    const std::uint64_t sampleIndex = renderSample_ + static_cast<std::uint64_t>(i);
    float leftMix = 0.0f;
    float rightMix = 0.0f;

    for (auto& voice : voices_) {
      if (!voice.active || sampleIndex < voice.startSample) {
        continue;
      }

      const float t = static_cast<float>(sampleIndex - voice.startSample) / sampleRate_;
      const float env = voice.amplitude * std::exp(-t / std::max(voice.decaySeconds, 0.01f));
      if (env < 1e-4f) {
        voice.active = false;
        continue;
      }

      const float phase = voice.phase + kTwoPi * voice.frequency * t;
      const float sample = std::sin(phase) * env;
      leftMix += sample * voice.leftGain;
      rightMix += sample * voice.rightGain;
    }

    ctx.left[i] += leftMix;
    ctx.right[i] += rightMix;
  }

  renderSample_ += ctx.frames;
#endif
}

Engine::StateBuffer ToyGenerator::serializeState() const {
  return {};
}

void ToyGenerator::deserializeState(const StateBuffer& state) {
  (void)state;
}

void ToyGenerator::panic() {
  voices_.fill(Voice{});
  nextVoice_ = 0;
  renderSample_ = 0;
}

void ToyGenerator::trigger(const Seed& seed, std::uint32_t whenSamples) {
  Voice& voice = voices_[nextVoice_ % kMaxVoices];
  nextVoice_ = (nextVoice_ + 1) % kMaxVoices;

  const float pitchRatio = semitoneToRatio(seed.pitch);
  const float base = kBaseHz * pitchRatio;
  const float toneShift = 0.5f + 0.75f * std::clamp(seed.tone, 0.0f, 1.0f);
  voice.frequency = base * toneShift;
  voice.startSample = whenSamples;
  voice.phase = 0.0f;
  voice.amplitude = kMaxAmplitude * std::clamp(seed.probability, 0.1f, 1.0f);
  voice.decaySeconds = 0.15f + std::clamp(seed.envR, 0.0f, 0.5f);

  const float pan = std::clamp((seed.spread - 0.5f) * 1.4f, -0.9f, 0.9f);
  voice.leftGain = 0.5f * (1.0f - pan);
  voice.rightGain = 0.5f * (1.0f + pan);
  voice.active = true;
}


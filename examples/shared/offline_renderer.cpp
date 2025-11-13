#include "offline_renderer.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <sstream>
#include <string>
#include <type_traits>

#include "SeedBoxConfig.h"
#include "engine/Resonator.h"
#include "engine/Sampler.h"

namespace offline {
namespace {

constexpr double kTwoPi = 6.283185307179586476925286766559;

double samplerAdsr(double t,
                   double attack,
                   double decay,
                   double sustain,
                   double release,
                   double sustainHold) {
  if (t < 0.0) {
    return 0.0;
  }

  constexpr double kMinStage = 1e-4;
  attack = std::max(attack, kMinStage);
  decay = std::max(decay, kMinStage);
  release = std::max(release, kMinStage);

  if (t < attack) {
    return t / attack;
  }
  t -= attack;
  if (t < decay) {
    const double progress = t / decay;
    return 1.0 + (sustain - 1.0) * progress;
  }
  t -= decay;
  if (t < sustainHold) {
    return sustain;
  }
  t -= sustainHold;
  if (t < release) {
    const double progress = t / release;
    return sustain * (1.0 - progress);
  }
  return 0.0;
}

double modalEnvelope(double t, double damping, double feedback) {
  const double sustain = std::clamp(0.35 + 0.45 * feedback, 0.1, 0.95);
  const double decayHz = 0.75 + (1.5 - damping) * 1.75;
  return sustain * std::exp(-t * decayHz);
}

void writeTag(std::ofstream& out, const char (&tag)[5]) {
  out.write(tag, 4);
}

template <typename T>
void writeLittleEndian(std::ofstream& out, T value) {
  using UnsignedT = std::make_unsigned_t<T>;
  UnsignedT v = static_cast<UnsignedT>(value);
  for (std::size_t i = 0; i < sizeof(UnsignedT); ++i) {
    const char byte = static_cast<char>((v >> (8u * i)) & 0xFFu);
    out.put(byte);
  }
}

}  // namespace

OfflineRenderer::OfflineRenderer(RenderSettings settings)
    : settings_(settings) {
  reset();
}

void OfflineRenderer::reset() {
  mix_.assign(settings_.frames, 0.0);
  pcm16_.clear();
}

void OfflineRenderer::ensureBuffer(std::size_t framesNeeded) {
  if (framesNeeded > mix_.size()) {
    mix_.resize(framesNeeded, 0.0);
  }
}

void OfflineRenderer::mixSamplerEvents(const std::vector<SamplerEvent>& events) {
  if (events.empty()) {
    return;
  }

  Sampler sampler;
  sampler.init();
  std::uint32_t maxWhen = 0;
  for (const auto& evt : events) {
    sampler.trigger(evt.seed, evt.whenSamples);
    maxWhen = std::max(maxWhen, evt.whenSamples);
  }
  const std::size_t tail = static_cast<std::size_t>(settings_.sampleRate * 2.0);
  ensureBuffer(static_cast<std::size_t>(maxWhen) + tail + 1);

  const double framesPerSecond = settings_.sampleRate;
  const double sustainHold = settings_.samplerSustainHold;
  const double baseFrequencies[] = {110.0, 164.81, 220.0, 261.63, 329.63, 392.0, 523.25};

  for (std::uint8_t i = 0; i < Sampler::kMaxVoices; ++i) {
    const auto voice = sampler.voice(i);
    if (!voice.active) {
      continue;
    }

    const double freqBase = baseFrequencies[voice.sampleIndex % std::size(baseFrequencies)];
    const double freq = freqBase * static_cast<double>(voice.playbackRate);
    const double pan = 0.5 * (static_cast<double>(voice.leftGain) + static_cast<double>(voice.rightGain));
    const std::size_t start = static_cast<std::size_t>(voice.startSample);

    for (std::size_t frame = start; frame < mix_.size(); ++frame) {
      const double t = (static_cast<double>(frame - start)) / framesPerSecond;
      const double env = samplerAdsr(t,
                                     static_cast<double>(voice.envelope.attack),
                                     static_cast<double>(voice.envelope.decay),
                                     static_cast<double>(voice.envelope.sustain),
                                     static_cast<double>(voice.envelope.release),
                                     sustainHold);
      if (env <= 0.0) {
        if (t > (voice.envelope.attack + voice.envelope.decay + sustainHold + voice.envelope.release)) {
          break;
        }
        continue;
      }

      const double toneBlend = static_cast<double>(voice.tone);
      const double fundamental = std::sin(kTwoPi * freq * t);
      const double harmonic = std::sin(kTwoPi * freq * 2.03 * t);
      double sample = (1.0 - toneBlend) * fundamental + toneBlend * harmonic;
      if (voice.usesSdStreaming) {
        const double grit = std::sin(kTwoPi * freq * 0.125 * t);
        sample = (sample * 0.9) + (grit * 0.1);
      }
      mix_[frame] += sample * env * pan;
    }
  }
}

void OfflineRenderer::mixResonatorEvents(const std::vector<ResonatorEvent>& events) {
  if (events.empty()) {
    return;
  }

  ResonatorBank bank;
  bank.init(ResonatorBank::Mode::kSim);
  std::uint32_t maxWhen = 0;
  for (const auto& evt : events) {
    bank.trigger(evt.seed, evt.whenSamples);
    maxWhen = std::max(maxWhen, evt.whenSamples);
  }
  const std::size_t tail = static_cast<std::size_t>(settings_.sampleRate * 4.0);
  ensureBuffer(static_cast<std::size_t>(maxWhen) + tail + 1);

  const double framesPerSecond = settings_.sampleRate;

  for (std::uint8_t i = 0; i < ResonatorBank::kMaxVoices; ++i) {
    const auto voice = bank.voice(i);
    if (!voice.active) {
      continue;
    }
    const std::size_t start = static_cast<std::size_t>(voice.startSample);
    const double burst = static_cast<double>(voice.burstGain);
    const double damping = static_cast<double>(voice.damping);
    const double feedback = static_cast<double>(voice.feedback);
    const double burstEnd = static_cast<double>(voice.burstMs) / 1000.0;

    for (std::size_t frame = start; frame < mix_.size(); ++frame) {
      const double t = (static_cast<double>(frame - start)) / framesPerSecond;
      const double envelope = modalEnvelope(t, damping, feedback);
      if (envelope < 1e-6) {
        if (t > 6.0) {
          break;
        }
        continue;
      }
      const double excite = std::exp(-std::max(0.0, t - burstEnd) * 6.5);
      double sample = 0.0;
      for (std::size_t mode = 0; mode < voice.modalFrequencies.size(); ++mode) {
        const double freq = static_cast<double>(voice.modalFrequencies[mode]);
        const double gain = static_cast<double>(voice.modalGains[mode]);
        if (gain <= 0.0 || freq <= 0.0) {
          continue;
        }
        sample += gain * std::sin(kTwoPi * freq * t);
      }
      sample += 0.35 * std::sin(kTwoPi * static_cast<double>(voice.frequency) * t);
      mix_[frame] += sample * burst * envelope * (0.5 + 0.5 * excite);
    }
  }
}

const std::vector<int16_t>& OfflineRenderer::finalize() {
  double maxAbs = 0.0;
  for (double v : mix_) {
    maxAbs = std::max(maxAbs, std::abs(v));
  }
  const double target = settings_.normalizeTarget;
  const double scale = (maxAbs > 0.0) ? (target / maxAbs) : 0.0;

  pcm16_.resize(mix_.size());
  for (std::size_t i = 0; i < mix_.size(); ++i) {
    const double scaled = mix_[i] * scale;
    const auto quantized = static_cast<long>(std::lround(scaled * 32767.0));
    pcm16_[i] = static_cast<int16_t>(std::clamp<long>(quantized, -32768L, 32767L));
  }
  return pcm16_;
}

bool OfflineRenderer::exportWav(const std::string& path,
                                std::uint32_t sampleRate,
                                const std::vector<int16_t>& samples) {
  if (path.empty() || sampleRate == 0 || samples.empty()) {
    return false;
  }

  std::filesystem::path p(path);
  if (p.has_parent_path()) {
    std::error_code ec;
    std::filesystem::create_directories(p.parent_path(), ec);
    if (ec) {
      return false;
    }
  }

  std::ofstream out(p, std::ios::binary);
  if (!out) {
    return false;
  }

  const std::uint16_t kChannels = 1;
  const std::uint16_t kBitsPerSample = 16;
  const std::uint32_t dataBytes = static_cast<std::uint32_t>(samples.size() * sizeof(std::int16_t));
  const std::uint32_t byteRate = sampleRate * kChannels * (kBitsPerSample / 8);
  const std::uint16_t blockAlign = static_cast<std::uint16_t>(kChannels * (kBitsPerSample / 8));

  writeTag(out, "RIFF");
  writeLittleEndian(out, static_cast<std::uint32_t>(36u + dataBytes));
  writeTag(out, "WAVE");
  writeTag(out, "fmt ");
  writeLittleEndian(out, static_cast<std::uint32_t>(16u));
  writeLittleEndian(out, static_cast<std::uint16_t>(1u));
  writeLittleEndian(out, kChannels);
  writeLittleEndian(out, sampleRate);
  writeLittleEndian(out, byteRate);
  writeLittleEndian(out, blockAlign);
  writeLittleEndian(out, kBitsPerSample);
  writeTag(out, "data");
  writeLittleEndian(out, dataBytes);
  out.write(reinterpret_cast<const char*>(samples.data()), dataBytes);
  return out.good();
}

bool OfflineRenderer::exportJson(const std::string& path, const std::string& payload) {
  if (path.empty()) {
    return false;
  }
  std::filesystem::path p(path);
  if (p.has_parent_path()) {
    std::error_code ec;
    std::filesystem::create_directories(p.parent_path(), ec);
    if (ec) {
      return false;
    }
  }
  std::ofstream out(p, std::ios::binary);
  if (!out) {
    return false;
  }
  out << payload;
  return out.good();
}

}  // namespace offline

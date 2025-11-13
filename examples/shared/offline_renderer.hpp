#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

#include "Seed.h"

namespace offline {

struct RenderSettings {
  double sampleRate = 48000.0;
  std::size_t frames = 48000;
  double samplerSustainHold = 0.25;
  double normalizeTarget = 0.92;
};

struct SamplerEvent {
  Seed seed;
  std::uint32_t whenSamples = 0;
};

struct ResonatorEvent {
  Seed seed;
  std::uint32_t whenSamples = 0;
};

class OfflineRenderer {
 public:
  explicit OfflineRenderer(RenderSettings settings = {});

  void reset();
  void mixSamplerEvents(const std::vector<SamplerEvent>& events);
  void mixResonatorEvents(const std::vector<ResonatorEvent>& events);
  const std::vector<int16_t>& finalize();

  const std::vector<double>& mixBuffer() const { return mix_; }
  double sampleRate() const { return settings_.sampleRate; }

  static bool exportWav(const std::string& path,
                        std::uint32_t sampleRate,
                        const std::vector<int16_t>& samples);
  static bool exportJson(const std::string& path, const std::string& payload);

 private:
  void ensureBuffer(std::size_t framesNeeded);

 private:
  RenderSettings settings_;
  std::vector<double> mix_;
  std::vector<int16_t> pcm16_;
};

}  // namespace offline

#pragma once
#include <array>
#include <cstdint>
#include "Seed.h"

// ResonatorBank sketches Option C (Karplus-Strong / modal ping engine). The
// actual Teensy Audio graph will plug in later; for now we keep the scheduling
// and per-seed parameter plumbing alive so the roadmap is executable.
class ResonatorBank {
public:
  enum class Mode : uint8_t { kSim, kHardware };

  void init(Mode mode);
  void setMaxVoices(uint8_t voices);
  void setDampingRange(float minDamping, float maxDamping);

  void trigger(const Seed& seed, uint32_t whenSamples);

  uint8_t activeVoices() const;

  static constexpr uint8_t kMaxVoices = 16;

private:
  struct Voice {
    bool active{false};
    uint32_t startSample{0};
    uint8_t seedId{0};
    float frequency{110.f};
    float damping{0.4f};
    float brightness{0.5f};
    float feedback{0.75f};
    uint8_t mode{0};
    uint8_t bank{0};
  };

  uint8_t allocateVoice();
  void planExcitation(Voice& v, const Seed& seed, uint32_t whenSamples);

private:
  Mode mode_{Mode::kSim};
  uint8_t maxVoices_{8};
  float minDamping_{0.25f};
  float maxDamping_{0.9f};
  std::array<Voice, kMaxVoices> voices_{};
};

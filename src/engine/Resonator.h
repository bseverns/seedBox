#pragma once
#include <array>
#include <cstdint>
#include "Seed.h"
#include "HardwarePrelude.h"
#include "util/Annotations.h"
#include <vector>

// ResonatorBank sketches Option C (Karplus-Strong / modal ping engine). The
// actual Teensy Audio graph will plug in later; for now we keep the scheduling
// and per-seed parameter plumbing alive so the roadmap is executable.
class ResonatorBank {
public:
  enum class Mode : uint8_t { kSim, kHardware };

  void init(Mode mode);
  SEEDBOX_MAYBE_UNUSED void setMaxVoices(uint8_t voices);
  SEEDBOX_MAYBE_UNUSED void setDampingRange(float minDamping, float maxDamping);

  SEEDBOX_MAYBE_UNUSED void trigger(const Seed& seed, uint32_t whenSamples);
  void onSeed(const Seed& seed);
  const Seed* lastSeed(uint32_t id) const;

  uint8_t activeVoices() const;
  SEEDBOX_MAYBE_UNUSED const char* presetName(uint8_t bank) const;

#ifdef SEEDBOX_HW
  SEEDBOX_MAYBE_UNUSED float fanoutProbeLevel() const;
#endif

  struct VoiceState {
    bool active{false};
    uint32_t handle{0};
    uint32_t startSample{0};
    uint8_t seedId{0};
    float frequency{110.f};
    float burstMs{3.5f};
    float damping{0.4f};
    float brightness{0.5f};
    float feedback{0.75f};
    float burstGain{1.0f};
    float delaySamples{0.0f};
    std::array<float, 4> modalFrequencies{};
    std::array<float, 4> modalGains{};
    uint8_t mode{0};
    uint8_t bank{0};
    const char* preset{nullptr};
  };

  SEEDBOX_MAYBE_UNUSED VoiceState voice(uint8_t voiceIndex) const;

  static constexpr uint8_t kMaxVoices = 16;

  struct ModalPreset {
    const char* name;
    std::array<float, 4> modeRatios;   // relative frequency multipliers
    std::array<float, 4> modeGains;    // gain per partial (0..1)
    float baseBrightness;              // tonal tilt default
    float baseFeedback;                // sustain baseline
  };

private:
  struct VoiceInternal {
    bool active{false};
    uint32_t handle{0};
    uint32_t startSample{0};
    uint8_t seedId{0};
    float frequency{110.f};
    float burstMs{3.5f};
    float damping{0.4f};
    float brightness{0.5f};
    float feedback{0.75f};
    float burstGain{1.0f};
    float delaySamples{0.0f};
    std::array<float, 4> modalFrequencies{};
    std::array<float, 4> modalGains{};
    uint8_t mode{0};
    uint8_t bank{0};
    const ModalPreset* preset{nullptr};
  };

  // Helper trio: choose a voice slot, translate the seed into modal parameters,
  // and then map that plan onto either hardware nodes or simulator state.
  uint8_t allocateVoice();
  void planExcitation(VoiceInternal& v, const Seed& seed, uint32_t whenSamples);
  void mapVoiceToGraph(uint8_t voiceIndex, VoiceInternal& voicePlan);
  const ModalPreset& resolvePreset(uint8_t bank) const;
  uint8_t clampMode(uint8_t requested) const;

private:
  Mode mode_{Mode::kSim};
  uint8_t maxVoices_{8};
  float minDamping_{0.25f};
  float maxDamping_{0.9f};
  std::array<VoiceInternal, kMaxVoices> voices_{};
  uint32_t nextHandle_{1};
  std::array<ModalPreset, 6> presets_{};
  std::vector<Seed> seedCache_{};

#ifdef SEEDBOX_HW
  struct HardwareVoice {
    AudioSynthNoiseWhite burstNoise;
    AudioEffectEnvelope burstEnv;
    AudioFilterStateVariable brightnessFilter;
    AudioEffectDelay stringDelay;
    std::array<AudioFilterBiquad, 4> modalFilters;
    AudioMixer4 modalMix;
    AudioMixer4 mix;
  };

  static constexpr uint8_t kMixerFanIn = 4;
  static constexpr uint8_t kMixerGroups = (kMaxVoices + kMixerFanIn - 1) / kMixerFanIn;
  static constexpr uint8_t kSubmixCount = (kMixerGroups + kMixerFanIn - 1) / kMixerFanIn;

  std::array<HardwareVoice, kMaxVoices> hwVoices_{};
  std::array<AudioMixer4, kMixerGroups> voiceMixerLeft_{};
  std::array<AudioMixer4, kMixerGroups> voiceMixerRight_{};
  std::array<AudioMixer4, kSubmixCount> submixLeft_{};
  std::array<AudioMixer4, kSubmixCount> submixRight_{};
  AudioMixer4 finalMixLeft_{};
  AudioMixer4 finalMixRight_{};
  mutable AudioAnalyzePeak voiceFanoutProbe_{};
  AudioOutputI2S output_{};
  std::vector<std::unique_ptr<AudioConnection>> patchCables_{};
#endif
};

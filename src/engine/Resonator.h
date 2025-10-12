#pragma once
#include <array>
#include <cstdint>
#include "Seed.h"

#ifdef SEEDBOX_HW
#include "hal/ArduinoGlue.h"
#include <Audio.h>
#include <memory>
#include <vector>
#endif

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
  const char* presetName(uint8_t bank) const;

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

  VoiceState voice(uint8_t index) const;

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

  uint8_t allocateVoice();
  void planExcitation(VoiceInternal& v, const Seed& seed, uint32_t whenSamples);
  void mapVoiceToGraph(uint8_t index, VoiceInternal& voice);
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

  std::array<HardwareVoice, kMaxVoices> hwVoices_{};
  AudioMixer4 voiceMixerLeft_{};
  AudioMixer4 voiceMixerRight_{};
  AudioOutputI2S output_{};
  std::vector<std::unique_ptr<AudioConnection>> patchCables_{};
#endif
};

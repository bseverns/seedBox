#pragma once
#include <array>
#include <cstdint>
#include "Seed.h"
#include "HardwarePrelude.h"
#include "engine/Engine.h"
#include "util/Annotations.h"

// Planning scaffold for Option B (granular engine). We now spin up the Teensy
// Audio graph when running on hardware (and keep detailed stubs for the native
// sim) so the control surface, routing, and documentation all stay in lockstep.
// Seeds map deterministically into `GrainVoice` plans that tests can snapshot,
// making it possible to teach the whole DSP chain straight from the code.
class GranularEngine : public Engine {
public:
  enum class Mode : uint8_t { kSim, kHardware };
  enum class Source : uint8_t { kLiveInput = 0, kSdClip = 1 };

  struct GrainVoice {
    bool active{false};
    uint32_t startSample{0};
    uint32_t seedPrng{0};
    float playbackRate{1.f};
    float sizeMs{80.f};
    float sprayMs{0.f};
    float windowSkew{0.f};
    float stereoSpread{0.5f};
    float leftGain{0.f};
    float rightGain{0.f};
    Source source{Source::kLiveInput};
    const char* sourcePath{nullptr};
    uint16_t sourceHandle{0};
    uint16_t dspHandle{0};
    uint8_t sdSlot{0};
    uint8_t seedId{0};
  };

  GranularEngine() = default;

  void init(Mode mode);
  SEEDBOX_MAYBE_UNUSED void setMaxActiveVoices(uint8_t voices);
  SEEDBOX_MAYBE_UNUSED void armLiveInput(bool enabled);
  SEEDBOX_MAYBE_UNUSED void registerSdClip(uint8_t slot, const char* path);

  SEEDBOX_MAYBE_UNUSED void trigger(const Seed& seed, uint32_t whenSamples);

  // Engine interface -----------------------------------------------------
  void prepare(const Engine::PrepareContext& ctx) override;
  void onTick(const Engine::TickContext& ctx) override;
  void onParam(const Engine::ParamChange& change) override;
  void onSeed(const Engine::SeedContext& ctx) override;
  void renderAudio(const Engine::RenderContext& ctx) override;
  Engine::StateBuffer serializeState() const override;
  void deserializeState(const Engine::StateBuffer& state) override;

  uint8_t activeVoiceCount() const;
  SEEDBOX_MAYBE_UNUSED GrainVoice voice(uint8_t index) const;
  Mode mode() const { return mode_; }

#ifndef SEEDBOX_HW
  struct SimHardwareVoice {
    bool sdPlayerStopCalled{false};
    bool sdPlayerPlayCalled{false};
    bool sdPlayerPlaying{false};
    const char* lastPlayPath{nullptr};
  };

  SimHardwareVoice simHardwareVoice(uint8_t index) const;
#endif

  static constexpr uint8_t kVoicePoolSize = 40;
  // Grain windows live in DMAMEM so the TCM heap can breathe.  Keep this in sync
  // with the teensy granular effect's happy place.
  static constexpr int kGrainMemorySamples = 2048;
  static constexpr uint8_t kSdClipSlots = 8;

private:
  struct SourceSlot {
    bool inUse{false};
    Source type{Source::kSdClip};
    const char* path{nullptr};
    uint16_t handle{0};
  };

  // Helpers that break down the trigger path into teachable chunks:
  //   allocateVoice -> pick a slot,
  //   planGrain     -> translate seed genome into a GrainVoice record,
  //   mapGrainToGraph -> wire that plan into the Teensy graph (or simulator).
  uint8_t allocateVoice();
  void planGrain(GrainVoice& voice, const Seed& seed, uint32_t whenSamples);
  void mapGrainToGraph(uint8_t index, GrainVoice& grain);
  Source resolveSource(uint8_t encoded) const;
  const SourceSlot* resolveSourceSlot(Source source, uint8_t requestedSlot) const;

private:
  Mode mode_{Mode::kSim};
  uint8_t maxActiveVoices_{20};
  bool liveInputArmed_{true};
  std::array<GrainVoice, kVoicePoolSize> voices_{};
  std::array<SourceSlot, kSdClipSlots> sdClips_{};

#ifdef SEEDBOX_HW
  struct HardwareVoice {
    AudioPlaySdWav sdPlayer;
    AudioMixer4 sourceMixer;
    AudioEffectGranular granular;
  };

  static constexpr uint8_t kMixerFanIn = 4;
  static constexpr uint8_t kMixerGroups = (kVoicePoolSize + kMixerFanIn - 1) / kMixerFanIn;
  static constexpr uint8_t kSubmixCount = (kMixerGroups + kMixerFanIn - 1) / kMixerFanIn;

  AudioInputI2S liveInput_{};
  std::array<HardwareVoice, kVoicePoolSize> hwVoices_{};
  std::array<AudioMixer4, kMixerGroups> voiceMixerLeft_{};
  std::array<AudioMixer4, kMixerGroups> voiceMixerRight_{};
  std::array<AudioMixer4, kSubmixCount> submixLeft_{};
  std::array<AudioMixer4, kSubmixCount> submixRight_{};
  AudioMixer4 finalMixLeft_{};
  AudioMixer4 finalMixRight_{};
  AudioOutputI2S output_{};
  std::vector<std::unique_ptr<AudioConnection>> patchCables_{};
#else
  std::array<SimHardwareVoice, kVoicePoolSize> simHwVoices_{};
#endif
};

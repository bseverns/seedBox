#pragma once
#include <array>
#include <cstdint>
#include "Seed.h"

// Planning scaffold for Option B (granular engine). The real DSP voices are not
// implemented yet, but this class captures how seeds map into grain plans and
// how we budget CPU/voices on the Teensy. Keeping the control path alive now
// makes it trivial to bolt the Teensy Audio primitives on later.
class GranularEngine {
public:
  enum class Mode : uint8_t { kSim, kHardware };
  enum class Source : uint8_t { kLiveInput = 0, kSdClip = 1 };

  GranularEngine() = default;

  void init(Mode mode);
  void setMaxActiveVoices(uint8_t voices);
  void armLiveInput(bool enabled);
  void registerSdClip(uint8_t slot, const char* path);

  void trigger(const Seed& seed, uint32_t whenSamples);

  uint8_t activeVoiceCount() const;
  Mode mode() const { return mode_; }

  static constexpr uint8_t kVoicePoolSize = 40;
  static constexpr uint8_t kSdClipSlots = 8;

private:
  struct GrainVoice {
    bool active{false};
    uint32_t startSample{0};
    uint32_t seedPrng{0};
    float playbackRate{1.f};
    float sizeMs{80.f};
    float sprayMs{0.f};
    float windowSkew{0.f};
    float stereoSpread{0.5f};
    Source source{Source::kLiveInput};
    uint8_t sdSlot{0};
    uint8_t seedId{0};
  };

  struct SourceSlot {
    bool inUse{false};
    Source type{Source::kSdClip};
    const char* path{nullptr};
  };

  uint8_t allocateVoice();
  void planGrain(GrainVoice& voice, const Seed& seed, uint32_t whenSamples);
  Source resolveSource(uint8_t encoded) const;

private:
  Mode mode_{Mode::kSim};
  uint8_t maxActiveVoices_{20};
  bool liveInputArmed_{true};
  std::array<GrainVoice, kVoicePoolSize> voices_{};
  std::array<SourceSlot, kSdClipSlots> sdClips_{};
};

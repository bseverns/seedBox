//
// Granular.cpp
// -------------
// The granular engine planner.  Still very much a scaffolding layer, but packed
// with commentary so the eventual DSP hookup has a roadmap.  Students can follow
// this file end-to-end to see how a seed mutates into a grain plan and how that
// plan would patch into the Teensy audio graph.
#include "engine/Granular.h"
#include <algorithm>
#include <cmath>
#include <type_traits>
#include <utility>
#include "engine/Stereo.h"
#include "util/RNG.h"
#include "util/Units.h"

namespace {
static uint8_t clampVoices(uint8_t voices) {
  if (voices < 1) return 1;
  if (voices > GranularEngine::kVoicePoolSize) return GranularEngine::kVoicePoolSize;
  return voices;
}
}

#ifdef SEEDBOX_HW
namespace {

template <typename T, typename = void>
struct HasSetMix : std::false_type {};

template <typename T>
struct HasSetMix<T, std::void_t<decltype(std::declval<T&>().setMix(0.5f))>> : std::true_type {};

template <typename T, typename = void>
struct HasSetGrainLength : std::false_type {};

template <typename T>
struct HasSetGrainLength<T, std::void_t<decltype(std::declval<T&>().setGrainLength(1))>> : std::true_type {};

template <typename T, typename = void>
struct HasBeginPitchShift : std::false_type {};

template <typename T>
struct HasBeginPitchShift<T, std::void_t<decltype(std::declval<T&>().beginPitchShift(1.0f))>> : std::true_type {};

template <typename Effect>
void configureGranularWindow(Effect& effect, float windowSkew, float grainLengthMs, int grainLengthSamples) {
  if constexpr (HasSetMix<Effect>::value) {
    const float clampedWindow = std::max(-1.0f, std::min(1.0f, windowSkew));
    const float mix = (clampedWindow + 1.0f) * 0.5f;
    effect.setMix(mix);
  } else {
    (void)windowSkew;
  }

  if constexpr (HasSetGrainLength<Effect>::value) {
    effect.setGrainLength(grainLengthSamples);
  } else if constexpr (HasBeginPitchShift<Effect>::value) {
    effect.beginPitchShift(grainLengthMs);
  } else {
    (void)grainLengthMs;
  }
}

}  // namespace
#endif  // SEEDBOX_HW

void GranularEngine::init(Mode mode) {
  mode_ = mode;
  maxActiveVoices_ = (mode == Mode::kHardware) ? 32 : 12;
  liveInputArmed_ = true;
  for (auto &voice : voices_) {
    voice = GrainVoice{};
  }
  for (auto &slot : sdClips_) {
    slot = SourceSlot{};
  }
  // Slot zero is a reserved label for "live input" so deterministic seeds can
  // reference it even though it never appears in the SD clip registry.
  sdClips_[0].inUse = true;
  sdClips_[0].type = Source::kLiveInput;
  sdClips_[0].path = "live-in";
  sdClips_[0].handle = 0;

#ifndef SEEDBOX_HW
  simHwVoices_.fill(SimHardwareVoice{});
#endif

#ifdef SEEDBOX_HW
  patchCables_.clear();

  for (uint8_t i = 0; i < kVoicePoolSize; ++i) {
    auto &hw = hwVoices_[i];
    hw.sourceMixer.gain(0, 0.0f);
    hw.sourceMixer.gain(1, 0.0f);
    hw.granular.begin(hw.grainMemory, static_cast<int>(sizeof(hw.grainMemory) / sizeof(hw.grainMemory[0])));

    const uint8_t group = static_cast<uint8_t>(i / kMixerFanIn);
    const uint8_t slot = static_cast<uint8_t>(i % kMixerFanIn);

    voiceMixerLeft_[group].gain(slot, 0.0f);
    voiceMixerRight_[group].gain(slot, 0.0f);

    patchCables_.emplace_back(std::make_unique<AudioConnection>(liveInput_, 0, hw.sourceMixer, 0));
    patchCables_.emplace_back(std::make_unique<AudioConnection>(hw.sdPlayer, 0, hw.sourceMixer, 1));
    patchCables_.emplace_back(std::make_unique<AudioConnection>(hw.sourceMixer, 0, hw.granular, 0));
    patchCables_.emplace_back(std::make_unique<AudioConnection>(hw.granular, 0, voiceMixerLeft_[group], slot));
    patchCables_.emplace_back(std::make_unique<AudioConnection>(hw.granular, 0, voiceMixerRight_[group], slot));

    voices_[i].dspHandle = i;
  }

  for (uint8_t group = 0; group < kMixerGroups; ++group) {
    const uint8_t submixIndex = static_cast<uint8_t>(group / kMixerFanIn);
    const uint8_t submixSlot = static_cast<uint8_t>(group % kMixerFanIn);
    submixLeft_[submixIndex].gain(submixSlot, 1.0f);
    submixRight_[submixIndex].gain(submixSlot, 1.0f);
    patchCables_.emplace_back(std::make_unique<AudioConnection>(voiceMixerLeft_[group], 0, submixLeft_[submixIndex], submixSlot));
    patchCables_.emplace_back(std::make_unique<AudioConnection>(voiceMixerRight_[group], 0, submixRight_[submixIndex], submixSlot));
  }

  for (uint8_t submix = 0; submix < kSubmixCount; ++submix) {
    finalMixLeft_.gain(submix, 1.0f);
    finalMixRight_.gain(submix, 1.0f);
    patchCables_.emplace_back(std::make_unique<AudioConnection>(submixLeft_[submix], 0, finalMixLeft_, submix));
    patchCables_.emplace_back(std::make_unique<AudioConnection>(submixRight_[submix], 0, finalMixRight_, submix));
  }

  patchCables_.emplace_back(std::make_unique<AudioConnection>(finalMixLeft_, 0, output_, 0));
  patchCables_.emplace_back(std::make_unique<AudioConnection>(finalMixRight_, 0, output_, 1));
#endif
}

void GranularEngine::setMaxActiveVoices(uint8_t voices) {
  maxActiveVoices_ = clampVoices(voices);
}

void GranularEngine::armLiveInput(bool enabled) {
  liveInputArmed_ = enabled;
}

void GranularEngine::registerSdClip(uint8_t slot, const char* path) {
  if (slot >= kSdClipSlots) return;
  if (slot == 0) return; // zero stays reserved for live input alias
  sdClips_[slot].inUse = true;
  sdClips_[slot].type = Source::kSdClip;
  sdClips_[slot].path = path;
  sdClips_[slot].handle = slot;
}

uint8_t GranularEngine::activeVoiceCount() const {
  return static_cast<uint8_t>(std::count_if(voices_.begin(), voices_.begin() + static_cast<size_t>(maxActiveVoices_), [](const GrainVoice& v) {
    return v.active;
  }));
}

GranularEngine::GrainVoice GranularEngine::voice(uint8_t index) const {
  if (index >= kVoicePoolSize) {
    return GrainVoice{};
  }
  return voices_[index];
}

GranularEngine::Source GranularEngine::resolveSource(uint8_t encoded) const {
  if (encoded == static_cast<uint8_t>(Source::kLiveInput)) {
    return liveInputArmed_ ? Source::kLiveInput : Source::kSdClip;
  }
  if (encoded >= kSdClipSlots) {
    return Source::kSdClip;
  }
  const SourceSlot& slot = sdClips_[encoded];
  if (!slot.inUse) {
    return Source::kSdClip;
  }
  return slot.type;
}

const GranularEngine::SourceSlot* GranularEngine::resolveSourceSlot(Source source, uint8_t requestedSlot) const {
  if (source == Source::kLiveInput) {
    return &sdClips_[0];
  }
  if (requestedSlot < kSdClipSlots) {
    const SourceSlot& slot = sdClips_[requestedSlot];
    if (slot.inUse && slot.type == Source::kSdClip) {
      return &slot;
    }
  }
  // Fall back to the first populated SD slot so a missing sample reference
  // doesn't result in silence.
  for (uint8_t i = 1; i < kSdClipSlots; ++i) {
    const SourceSlot& slot = sdClips_[i];
    if (slot.inUse && slot.type == Source::kSdClip) {
      return &slot;
    }
  }
  return nullptr;
}

uint8_t GranularEngine::allocateVoice() {
  // First hunt for an idle voice.
  for (uint8_t i = 0; i < maxActiveVoices_; ++i) {
    if (!voices_[i].active) {
      return i;
    }
  }

  // If all voices are active we just steal the oldest one. This keeps the
  // control code deterministic even before the DSP stage exists.
  uint8_t oldest = 0;
  uint32_t minSample = voices_[0].startSample;
  for (uint8_t i = 1; i < maxActiveVoices_; ++i) {
    if (voices_[i].startSample < minSample) {
      oldest = i;
      minSample = voices_[i].startSample;
    }
  }
  return oldest;
}

void GranularEngine::planGrain(GrainVoice& voice, const Seed& seed, uint32_t whenSamples) {
  voice.active = true;
  voice.startSample = whenSamples;
  voice.seedId = static_cast<uint8_t>(seed.id);
  voice.sizeMs = seed.granular.grainSizeMs;
  voice.sprayMs = seed.granular.sprayMs;
  voice.windowSkew = seed.granular.windowSkew;
  voice.stereoSpread = seed.granular.stereoSpread;
  voice.source = resolveSource(seed.granular.source);
  voice.sdSlot = seed.granular.sdSlot;

  const SourceSlot* resolved = resolveSourceSlot(voice.source, voice.sdSlot);
  if (resolved != nullptr) {
    voice.sourcePath = resolved->path;
    voice.sourceHandle = resolved->handle;
  } else {
    voice.sourcePath = nullptr;
    voice.sourceHandle = 0;
  }

  uint32_t prng = seed.prng;

  // playbackRate pulls from both the seed pitch (global) and granular
  // transpose (local per-engine). RNG seeded with the per-seed PRNG keeps
  // things deterministic even when sprayMs is non-zero.
  float semitones = seed.pitch + seed.granular.transpose;
  voice.playbackRate = 1.0f;
  if (semitones != 0.f) {
    voice.playbackRate = std::pow(2.0f, semitones / 12.0f);
  }

  if (voice.sprayMs > 0.f) {
    const float spray = (RNG::uniformSigned(prng) * voice.sprayMs);
    const uint32_t offset = Units::msToSamples(std::abs(spray));
    if (spray >= 0.f) {
      voice.startSample += offset;
    } else {
      voice.startSample = (voice.startSample > offset) ? (voice.startSample - offset) : 0u;
    }
  }

  voice.seedPrng = prng;
}

void GranularEngine::mapGrainToGraph(uint8_t index, GrainVoice& voice) {
  const auto gains = stereo::constantPowerWidth(voice.stereoSpread);
  voice.leftGain = gains.left;
  voice.rightGain = gains.right;

#ifdef SEEDBOX_HW
  auto& hw = hwVoices_[index];
  hw.sdPlayer.stop();
  hw.sourceMixer.gain(0, voice.source == Source::kLiveInput ? 1.0f : 0.0f);
  hw.sourceMixer.gain(1, voice.source == Source::kSdClip ? 1.0f : 0.0f);

  if (voice.source == Source::kSdClip && voice.sourcePath != nullptr) {
    hw.sdPlayer.play(voice.sourcePath);
  }

  hw.granular.setSpeed(voice.playbackRate);
  const float grainLengthMs = std::max(1.0f, voice.sizeMs);
  const int grainLengthSamples = static_cast<int>(Units::msToSamples(grainLengthMs));
  configureGranularWindow(hw.granular, voice.windowSkew, grainLengthMs, grainLengthSamples);

  const uint8_t group = static_cast<uint8_t>(index / kMixerFanIn);
  const uint8_t slot = static_cast<uint8_t>(index % kMixerFanIn);
  voiceMixerLeft_[group].gain(slot, voice.leftGain);
  voiceMixerRight_[group].gain(slot, voice.rightGain);
#else
  auto& sim = simHwVoices_[index];
  sim.sdPlayerStopCalled = true;
  sim.sdPlayerPlayCalled = false;
  sim.sdPlayerPlaying = false;
  sim.lastPlayPath = nullptr;

  if (voice.source == Source::kSdClip && voice.sourcePath != nullptr) {
    sim.sdPlayerPlayCalled = true;
    sim.sdPlayerPlaying = true;
    sim.lastPlayPath = voice.sourcePath;
  }
#endif
}

void GranularEngine::trigger(const Seed& seed, uint32_t whenSamples) {
  if (maxActiveVoices_ == 0) {
    return;
  }
  uint8_t voiceIndex = allocateVoice();
  planGrain(voices_[voiceIndex], seed, whenSamples);
  voices_[voiceIndex].dspHandle = voiceIndex;
  // Whether we're on hardware or in the simulator, this final step ties the
  // planned grain into something that will eventually make sound.
  mapGrainToGraph(voiceIndex, voices_[voiceIndex]);
}

#ifndef SEEDBOX_HW
GranularEngine::SimHardwareVoice GranularEngine::simHardwareVoice(uint8_t index) const {
  if (index >= simHwVoices_.size()) {
    return SimHardwareVoice{};
  }
  return simHwVoices_[index];
}
#endif

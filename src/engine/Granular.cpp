#include "engine/Granular.h"
#include <algorithm>
#include <cmath>
#include "util/RNG.h"
#include "util/Units.h"

namespace {
static uint8_t clampVoices(uint8_t voices) {
  if (voices < 1) return 1;
  if (voices > GranularEngine::kVoicePoolSize) return GranularEngine::kVoicePoolSize;
  return voices;
}
}

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
}

uint8_t GranularEngine::activeVoiceCount() const {
  return static_cast<uint8_t>(std::count_if(voices_.begin(), voices_.begin() + static_cast<size_t>(maxActiveVoices_), [](const GrainVoice& v) {
    return v.active;
  }));
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

void GranularEngine::trigger(const Seed& seed, uint32_t whenSamples) {
  if (maxActiveVoices_ == 0) {
    return;
  }
  uint8_t voiceIndex = allocateVoice();
  planGrain(voices_[voiceIndex], seed, whenSamples);

  // Once the DSP side lands we will push the voice plan into the Teensy Audio
  // objects here. For now we just keep the state machine hot so scheduling and
  // documentation can evolve in parallel.
}

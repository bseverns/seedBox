//
// Granular.cpp
// -------------
// The granular engine planner.  Still very much a scaffolding layer, but packed
// with commentary so the eventual DSP hookup has a roadmap.  Students can follow
// this file end-to-end to see how a seed mutates into a grain plan and how that
// plan would patch into the Teensy audio graph.
#include "engine/Granular.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <type_traits>
#include <utility>
#include "engine/Stereo.h"
#include "util/RNG.h"
#include "util/Units.h"

namespace {

#if SEEDBOX_HW
// Teensy shoves DMAMEM symbols into RAM2, which keeps these hulking grain
// buffers out of the precious tightly-coupled RAM1 pool.  Each voice grabs a
// 2048-sample window during init and recycles it forever.
DMAMEM int16_t gGranularGrainPool[GranularEngine::kVoicePoolSize][GranularEngine::kGrainMemorySamples];
#endif

constexpr std::array<float, GranularEngine::Stats::kHistogramBins> kSizeBinEdgesMs{{10.f, 25.f, 50.f, 100.f, 200.f, 400.f}};
constexpr std::array<float, GranularEngine::Stats::kHistogramBins> kSprayBinEdgesMs{{0.5f, 5.f, 15.f, 30.f, 60.f, 120.f}};

template <typename Array>
uint8_t bucketForValue(float value, const Array& edges) {
  const float clamped = std::max(0.0f, value);
  for (uint8_t i = 0; i < edges.size(); ++i) {
    if (clamped <= edges[i]) {
      return i;
    }
  }
  return static_cast<uint8_t>(edges.size() - 1);
}

static uint8_t clampVoices(uint8_t voices) {
  if (voices < 1) return 1;
  if (voices > GranularEngine::kVoicePoolSize) return GranularEngine::kVoicePoolSize;
  return voices;
}
}

#if SEEDBOX_HW
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

void GranularEngine::Stats::reset() {
  activeVoiceCount = 0;
  sdOnlyVoiceCount = 0;
  grainsPlanned = 0;
  grainSizeHistogram.fill(0);
  sprayHistogram.fill(0);
  mixerGroupLoad.fill(0);
  mixerGroupsEngaged = 0;
  busiestMixerGroup = 0;
  busiestMixerLoad = 0;
  voiceSamples_.fill(VoiceSample{});
}

void GranularEngine::Stats::onVoicePlanned(uint8_t voiceIndex, const GrainVoice& voice) {
  if (voiceIndex >= voiceSamples_.size()) {
    return;
  }

  auto& slot = voiceSamples_[voiceIndex];
  if (slot.active) {
    if (activeVoiceCount > 0) {
      --activeVoiceCount;
    }
    if (slot.sizeBin < grainSizeHistogram.size() && grainSizeHistogram[slot.sizeBin] > 0) {
      --grainSizeHistogram[slot.sizeBin];
    }
    if (slot.sprayBin < sprayHistogram.size() && sprayHistogram[slot.sprayBin] > 0) {
      --sprayHistogram[slot.sprayBin];
    }
    if (slot.sdOnly && sdOnlyVoiceCount > 0) {
      --sdOnlyVoiceCount;
    }
    if (slot.mixerGroup < mixerGroupLoad.size() && mixerGroupLoad[slot.mixerGroup] > 0) {
      --mixerGroupLoad[slot.mixerGroup];
    }
  }

  slot = VoiceSample{};
  ++grainsPlanned;

  if (!voice.active) {
    refreshMixerAggregates();
    return;
  }

  slot.active = true;
  slot.sizeBin = bucketForValue(voice.sizeMs, kSizeBinEdgesMs);
  slot.sprayBin = bucketForValue(voice.sprayMs, kSprayBinEdgesMs);
  slot.sdOnly = (voice.seedSource == Source::kSdClip);
  slot.mixerGroup = static_cast<uint8_t>(voiceIndex / kMixerFanIn);

  ++activeVoiceCount;
  ++grainSizeHistogram[slot.sizeBin];
  ++sprayHistogram[slot.sprayBin];
  if (slot.sdOnly) {
    ++sdOnlyVoiceCount;
  }
  if (slot.mixerGroup < mixerGroupLoad.size()) {
    ++mixerGroupLoad[slot.mixerGroup];
  }
  refreshMixerAggregates();
}

void GranularEngine::Stats::refreshMixerAggregates() {
  mixerGroupsEngaged = 0;
  busiestMixerGroup = 0;
  busiestMixerLoad = 0;
  for (uint8_t i = 0; i < mixerGroupLoad.size(); ++i) {
    const uint8_t load = mixerGroupLoad[i];
    if (load > 0) {
      ++mixerGroupsEngaged;
    }
    if (load > busiestMixerLoad) {
      busiestMixerLoad = load;
      busiestMixerGroup = i;
    }
  }
}

Engine::Type GranularEngine::type() const noexcept { return Engine::Type::kGranular; }

void GranularEngine::init(Mode mode) {
  mode_ = mode;
  maxActiveVoices_ = (mode == Mode::kHardware) ? 32 : 12;
  liveInputArmed_ = true;
  voices_.fill(GrainVoice{});
  sdClips_.fill(SourceSlot{});
  stats_.reset();
  // Slot zero is a reserved label for "live input" so deterministic seeds can
  // reference it even though it never appears in the SD clip registry.
  sdClips_[0].inUse = true;
  sdClips_[0].type = Source::kLiveInput;
  sdClips_[0].path = "live-in";
  sdClips_[0].handle = 0;

#if !SEEDBOX_HW
  simHwVoices_.fill(SimHardwareVoice{});
#endif

#if SEEDBOX_HW
  patchCables_.clear();

  for (uint8_t i = 0; i < kVoicePoolSize; ++i) {
    auto &hwVoice = hwVoices_[i];
    hwVoice.sourceMixer.gain(0, 0.0f);
    hwVoice.sourceMixer.gain(1, 0.0f);
    hwVoice.granular.begin(gGranularGrainPool[i], GranularEngine::kGrainMemorySamples);

    const uint8_t group = static_cast<uint8_t>(i / kMixerFanIn);
    const uint8_t slot = static_cast<uint8_t>(i % kMixerFanIn);

    voiceMixerLeft_[group].gain(slot, 0.0f);
    voiceMixerRight_[group].gain(slot, 0.0f);

    patchCables_.emplace_back(std::make_unique<AudioConnection>(liveInput_, 0, hwVoice.sourceMixer, 0));
    patchCables_.emplace_back(std::make_unique<AudioConnection>(hwVoice.sdPlayer, 0, hwVoice.sourceMixer, 1));
    patchCables_.emplace_back(std::make_unique<AudioConnection>(hwVoice.sourceMixer, 0, hwVoice.granular, 0));
    patchCables_.emplace_back(std::make_unique<AudioConnection>(hwVoice.granular, 0, voiceMixerLeft_[group], slot));
    patchCables_.emplace_back(std::make_unique<AudioConnection>(hwVoice.granular, 0, voiceMixerRight_[group], slot));

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

void GranularEngine::prepare(const Engine::PrepareContext& ctx) {
  init(ctx.hardware ? Mode::kHardware : Mode::kSim);
  (void)ctx.masterSeed;
  (void)ctx.sampleRate;
  (void)ctx.framesPerBlock;
}

void GranularEngine::onTick(const Engine::TickContext& ctx) {
  (void)ctx;
}

void GranularEngine::onParam(const Engine::ParamChange& change) {
  (void)change;
}

void GranularEngine::onSeed(const Engine::SeedContext& ctx) {
  trigger(ctx.seed, ctx.whenSamples);
}

void GranularEngine::renderAudio(const Engine::RenderContext& ctx) {
  (void)ctx;
}

Engine::StateBuffer GranularEngine::serializeState() const {
  return {};
}

void GranularEngine::deserializeState(const Engine::StateBuffer& state) {
  (void)state;
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
  voice.seedSource = static_cast<Source>(seed.granular.source);
  if (voice.seedSource != Source::kSdClip) {
    voice.seedSource = Source::kLiveInput;
  }
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

void GranularEngine::mapGrainToGraph(uint8_t index, GrainVoice& grain) {
  const auto gains = stereo::constantPowerWidth(grain.stereoSpread);
  grain.leftGain = gains.left;
  grain.rightGain = gains.right;

#if SEEDBOX_HW
  auto& hwVoice = hwVoices_[index];
  hwVoice.sdPlayer.stop();
  hwVoice.sourceMixer.gain(0, grain.source == Source::kLiveInput ? 1.0f : 0.0f);
  hwVoice.sourceMixer.gain(1, grain.source == Source::kSdClip ? 1.0f : 0.0f);

  if (grain.source == Source::kSdClip && grain.sourcePath != nullptr) {
    hwVoice.sdPlayer.play(grain.sourcePath);
  }

  hwVoice.granular.setSpeed(grain.playbackRate);
  const float grainLengthMs = std::max(1.0f, grain.sizeMs);
  const int grainLengthSamples = static_cast<int>(Units::msToSamples(grainLengthMs));
  configureGranularWindow(hwVoice.granular, grain.windowSkew, grainLengthMs, grainLengthSamples);

  const uint8_t group = static_cast<uint8_t>(index / kMixerFanIn);
  const uint8_t slot = static_cast<uint8_t>(index % kMixerFanIn);
  voiceMixerLeft_[group].gain(slot, grain.leftGain);
  voiceMixerRight_[group].gain(slot, grain.rightGain);
#else
  auto& sim = simHwVoices_[index];
  sim.sdPlayerStopCalled = true;
  sim.sdPlayerPlayCalled = false;
  sim.sdPlayerPlaying = false;
  sim.lastPlayPath = nullptr;

  if (grain.source == Source::kSdClip && grain.sourcePath != nullptr) {
    sim.sdPlayerPlayCalled = true;
    sim.sdPlayerPlaying = true;
    sim.lastPlayPath = grain.sourcePath;
  }
#endif
}

void GranularEngine::trigger(const Seed& seed, uint32_t whenSamples) {
  if (maxActiveVoices_ == 0) {
    return;
  }
  uint8_t voiceIndex = allocateVoice();
  planGrain(voices_[voiceIndex], seed, whenSamples);
  stats_.onVoicePlanned(voiceIndex, voices_[voiceIndex]);
  voices_[voiceIndex].dspHandle = voiceIndex;
  // Whether we're on hardware or in the simulator, this final step ties the
  // planned grain into something that will eventually make sound.
  mapGrainToGraph(voiceIndex, voices_[voiceIndex]);
}

#if !SEEDBOX_HW
GranularEngine::SimHardwareVoice GranularEngine::simHardwareVoice(uint8_t index) const {
  if (index >= simHwVoices_.size()) {
    return SimHardwareVoice{};
  }
  return simHwVoices_[index];
}
#endif

void GranularEngine::onSeed(const Seed& seed) {
  const std::size_t index = static_cast<std::size_t>(seed.id);
  if (seedCache_.size() <= index) {
    seedCache_.resize(index + 1);
  }
  seedCache_[index] = seed;
}

const Seed* GranularEngine::lastSeed(uint32_t id) const {
  const std::size_t index = static_cast<std::size_t>(id);
  if (index >= seedCache_.size()) {
    return nullptr;
  }
  return &seedCache_[index];
}

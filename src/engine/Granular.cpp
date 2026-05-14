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

#if !SEEDBOX_HW
float clampUnit(float value) {
  return std::clamp(value, 0.0f, 1.0f);
}

float mixLinear(float a, float b, float mix) {
  const float clamped = clampUnit(mix);
  return a + ((b - a) * clamped);
}

std::size_t wrapDelayTap(std::size_t writePos, std::size_t bufferSize, std::size_t delaySamples) {
  if (bufferSize == 0) {
    return 0;
  }
  const std::size_t clampedDelay = std::min(delaySamples, bufferSize - 1);
  return (writePos + bufferSize - clampedDelay) % bufferSize;
}
#endif
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
  hostSampleRate_ = 48000.0f;
  maxActiveVoices_ = (mode == Mode::kHardware) ? 32 : 12;
  liveInputArmed_ = true;
  voices_.fill(GrainVoice{});
  sdClips_.fill(SourceSlot{});
  stats_.reset();
  effectDelayLeft_.clear();
  effectDelayRight_.clear();
  effectWritePos_ = 0;
  effectLowpassLeft_ = 0.0f;
  effectLowpassRight_ = 0.0f;
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
  hostSampleRate_ = (ctx.sampleRate > 0) ? static_cast<float>(ctx.sampleRate) : 48000.0f;
  ensureEffectBuffers(ctx.framesPerBlock);
  (void)ctx.masterSeed;
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

void GranularEngine::processInputAudio(const Seed& seed, const Engine::RenderContext& ctx) {
#if SEEDBOX_HW
  (void)seed;
  (void)ctx;
#else
  if (!ctx.inputLeft || !ctx.left || !ctx.right || ctx.frames == 0) {
    return;
  }

  ensureEffectBuffers(ctx.frames);
  if (effectDelayLeft_.empty() || effectDelayRight_.empty()) {
    return;
  }

  const float tone = clampUnit(seed.tone);
  const float spread = clampUnit(seed.granular.stereoSpread);
  const float density = std::max(0.5f, seed.density);
  const float wet = 0.82f + (0.15f * clampUnit(seed.probability));
  const float feedback = 0.28f + (0.45f * clampUnit(seed.mutateAmt));
  const float lpCoeff = 0.04f + (tone * 0.32f);
  const float transposeMix = clampUnit((seed.granular.transpose + 12.0f) / 24.0f);
  const std::size_t delaySize = effectDelayLeft_.size();
  const std::size_t grainDelay = std::clamp<std::size_t>(
      static_cast<std::size_t>(Units::msToSamples(std::max(12.0f, seed.granular.grainSizeMs))), 24u, delaySize / 3u);
  const std::size_t sprayDelay = std::clamp<std::size_t>(
      static_cast<std::size_t>(Units::msToSamples(std::max(4.0f, seed.granular.sprayMs + (density * 6.0f)))), 16u,
      delaySize / 2u);
  const float windowBlend = clampUnit((seed.granular.windowSkew + 1.0f) * 0.5f);
  const float width = 0.12f + (0.6f * spread);
  const float transientBlend = 0.45f + (0.4f * windowBlend);
  const float haloBlend = 0.22f + (0.5f * spread);
  const float dryMix = 0.08f + (0.16f * (1.0f - clampUnit(seed.probability)));
  const float drive = 1.15f + (0.85f * clampUnit(seed.mutateAmt));
  const auto gains = stereo::constantPowerWidth(width);

  for (std::size_t i = 0; i < ctx.frames; ++i) {
    const float inL = ctx.inputLeft[i];
    const float inR = ctx.inputRight ? ctx.inputRight[i] : inL;
    const std::size_t writePos = effectWritePos_;
    const std::size_t tapA = wrapDelayTap(writePos, delaySize, grainDelay);
    const std::size_t tapB = wrapDelayTap(writePos, delaySize, grainDelay + sprayDelay);

    effectLowpassLeft_ += lpCoeff * (inL - effectLowpassLeft_);
    effectLowpassRight_ += lpCoeff * (inR - effectLowpassRight_);

    const float smearL = mixLinear(effectDelayLeft_[tapA], effectDelayLeft_[tapB], windowBlend);
    const float smearR = mixLinear(effectDelayRight_[tapA], effectDelayRight_[tapB], windowBlend);
    const float pitchedL = mixLinear(smearL, effectLowpassLeft_, transposeMix);
    const float pitchedR = mixLinear(smearR, effectLowpassRight_, transposeMix);
    const float transientL = inL - effectLowpassLeft_;
    const float transientR = inR - effectLowpassRight_;
    const float grainL = mixLinear(smearL, transientL, transientBlend);
    const float grainR = mixLinear(smearR, transientR, transientBlend);
    const float haloL = mixLinear(pitchedL, transientR, haloBlend);
    const float haloR = mixLinear(pitchedR, transientL, haloBlend);
    const float wetCoreL = (0.78f * grainL) + (0.52f * haloL) + (0.18f * effectLowpassLeft_);
    const float wetCoreR = (0.78f * grainR) + (0.52f * haloR) + (0.18f * effectLowpassRight_);
    const float shapedL = std::tanh(wetCoreL * drive);
    const float shapedR = std::tanh(wetCoreR * drive);
    const float stereoWetL = (shapedL * gains.left) + (shapedR * (1.0f - gains.right) * 0.5f);
    const float stereoWetR = (shapedR * gains.right) + (shapedL * (1.0f - gains.left) * 0.5f);
    const float outL = mixLinear(stereoWetL, inL, dryMix * (1.0f - wet));
    const float outR = mixLinear(stereoWetR, inR, dryMix * (1.0f - wet));

    ctx.left[i] += outL;
    ctx.right[i] += outR;

    effectDelayLeft_[writePos] = inL + (wetCoreL * feedback);
    effectDelayRight_[writePos] = inR + (wetCoreR * feedback);
    effectWritePos_ = (writePos + 1u) % delaySize;
  }
#endif
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

void GranularEngine::panic() {
  voices_.fill(GrainVoice{});
  stats_.reset();
  std::fill(effectDelayLeft_.begin(), effectDelayLeft_.end(), 0.0f);
  std::fill(effectDelayRight_.begin(), effectDelayRight_.end(), 0.0f);
  effectWritePos_ = 0;
  effectLowpassLeft_ = 0.0f;
  effectLowpassRight_ = 0.0f;
#if SEEDBOX_HW
  for (auto& hwVoice : hwVoices_) {
    hwVoice.sdPlayer.stop();
    hwVoice.sourceMixer.gain(0, 0.0f);
    hwVoice.sourceMixer.gain(1, 0.0f);
  }

  for (uint8_t group = 0; group < kMixerGroups; ++group) {
    for (uint8_t slot = 0; slot < kMixerFanIn; ++slot) {
      voiceMixerLeft_[group].gain(slot, 0.0f);
      voiceMixerRight_[group].gain(slot, 0.0f);
    }
  }

  for (uint8_t sub = 0; sub < kSubmixCount; ++sub) {
    for (uint8_t slot = 0; slot < kMixerFanIn; ++slot) {
      submixLeft_[sub].gain(slot, 0.0f);
      submixRight_[sub].gain(slot, 0.0f);
    }
  }

  for (uint8_t sub = 0; sub < kSubmixCount; ++sub) {
    finalMixLeft_.gain(sub, 0.0f);
    finalMixRight_.gain(sub, 0.0f);
  }
#else
  simHwVoices_.fill(SimHardwareVoice{});
#endif
}

void GranularEngine::setMaxActiveVoices(uint8_t voices) {
  maxActiveVoices_ = clampVoices(voices);
}

void GranularEngine::ensureEffectBuffers(std::size_t minFrames) {
  const float sr = (hostSampleRate_ > 0.0f) ? hostSampleRate_ : 48000.0f;
  const std::size_t desired = std::max<std::size_t>(static_cast<std::size_t>(sr * 2.0f), minFrames + 1u);
  if (effectDelayLeft_.size() == desired && effectDelayRight_.size() == desired) {
    return;
  }
  effectDelayLeft_.assign(desired, 0.0f);
  effectDelayRight_.assign(desired, 0.0f);
  effectWritePos_ = 0;
  effectLowpassLeft_ = 0.0f;
  effectLowpassRight_ = 0.0f;
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

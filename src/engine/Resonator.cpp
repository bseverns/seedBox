//
// Resonator.cpp
// -------------
// Modal synthesis playground.  We're still sketching but the comments aim to
// demystify how a Karplus-Strong style engine might look once the DSP comes on
// line.  Think of this as a guided tour from seed genome to modal voice plan.
#include "engine/Resonator.h"
#include <algorithm>
#include <array>
#include <cmath>
#include "util/RNG.h"
#include "util/Units.h"

Engine::Type ResonatorBank::type() const noexcept { return Engine::Type::kResonator; }

namespace {
float clamp01(float v) {
  return std::max(0.f, std::min(1.f, v));
}

float lerp(float a, float b, float t) {
  return a + (b - a) * t;
}

constexpr std::array<ResonatorBank::ModalPreset, 6> kDefaultPresets{{
    {"Brass shell",
     {1.0f, 2.01f, 2.55f, 3.9f},
     {1.0f, 0.62f, 0.48f, 0.3f},
     0.55f,
     0.82f},
    {"Glass harp",
     {1.0f, 1.5f, 2.5f, 3.5f},
     {0.9f, 0.7f, 0.5f, 0.35f},
     0.7f,
     0.74f},
    {"Kalimba tine",
     {1.0f, 2.0f, 3.0f, 4.2f},
     {1.0f, 0.5f, 0.35f, 0.2f},
     0.45f,
     0.68f},
    {"Chime tree",
     {1.0f, 2.63f, 3.91f, 5.02f},
     {0.95f, 0.55f, 0.4f, 0.32f},
     0.8f,
     0.86f},
    {"Aluminum bar",
     {1.0f, 3.0f, 5.8f, 9.2f},
     {1.0f, 0.52f, 0.38f, 0.24f},
     0.6f,
     0.9f},
    {"Detuned duo",
     {1.0f, 1.01f, 1.98f, 2.97f},
     {0.95f, 0.92f, 0.7f, 0.55f},
     0.5f,
     0.8f},
}};
}

void ResonatorBank::init(Mode mode) {
  mode_ = mode;
  maxVoices_ = (mode == Mode::kHardware) ? 10 : 4;
  nextHandle_ = 1;
  presets_ = kDefaultPresets;

  voices_.fill(VoiceInternal{});

#ifdef SEEDBOX_HW
  patchCables_.clear();

  for (uint8_t i = 0; i < kMaxVoices; ++i) {
    auto &hwVoice = hwVoices_[i];

    hwVoice.burstEnv.attack(0.25f);
    hwVoice.burstEnv.decay(3.0f);
    hwVoice.burstEnv.sustain(0.0f);
    hwVoice.burstEnv.release(2.0f);

    hwVoice.brightnessFilter.frequency(4000.0f);
    hwVoice.brightnessFilter.resonance(0.7f);

    hwVoice.stringDelay.delay(0, 10.0f);

    for (uint8_t m = 0; m < hwVoice.modalFilters.size(); ++m) {
      hwVoice.modalFilters[m].setBandpass(0, 800.0f + 200.0f * m, 1.4f);
      hwVoice.modalMix.gain(m, 0.0f);
    }

    hwVoice.mix.gain(0, 0.0f);
    hwVoice.mix.gain(1, 0.0f);

    patchCables_.emplace_back(std::make_unique<AudioConnection>(hwVoice.burstNoise, 0, hwVoice.burstEnv, 0));
    patchCables_.emplace_back(std::make_unique<AudioConnection>(hwVoice.burstEnv, 0, hwVoice.brightnessFilter, 0));
    patchCables_.emplace_back(std::make_unique<AudioConnection>(hwVoice.brightnessFilter, 0, hwVoice.stringDelay, 0));
    patchCables_.emplace_back(std::make_unique<AudioConnection>(hwVoice.stringDelay, 0, hwVoice.mix, 0));

    for (uint8_t m = 0; m < hwVoice.modalFilters.size(); ++m) {
      patchCables_.emplace_back(std::make_unique<AudioConnection>(hwVoice.brightnessFilter, 0, hwVoice.modalFilters[m], 0));
      patchCables_.emplace_back(std::make_unique<AudioConnection>(hwVoice.modalFilters[m], 0, hwVoice.modalMix, m));
    }

    patchCables_.emplace_back(std::make_unique<AudioConnection>(hwVoice.modalMix, 0, hwVoice.mix, 1));

    const uint8_t group = static_cast<uint8_t>(i / kMixerFanIn);
    const uint8_t slot = static_cast<uint8_t>(i % kMixerFanIn);

    patchCables_.emplace_back(std::make_unique<AudioConnection>(hwVoice.mix, 0, voiceMixerLeft_[group], slot));
    patchCables_.emplace_back(std::make_unique<AudioConnection>(hwVoice.mix, 0, voiceMixerRight_[group], slot));

    voiceMixerLeft_[group].gain(slot, 0.0f);
    voiceMixerRight_[group].gain(slot, 0.0f);
  }

  // Mixer topology sketch (because lab notes are half the fun):
  //   - Each resonator voice lands on a personal `hw.mix` node.
  //   - Four voices feed a group mixer block on each stereo side.
  //   - Up to four of those group sums fold into a submix.
  //   - The submix pair finally pours into the output codec.
  // This fan-out mirrors the granular engine's cascade so we can light up all
  // 16 hardware voices without overdriving a single AudioMixer4.
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

  if (kMixerGroups > 1) {
    // Hardware smoke test hook: tap the second voice group so firmware can
    // assert that voices 4-7 actually swing needles once we slam more than four
    // notes at a time. Platform bring-up code reads `fanoutProbeLevel()` and
    // howls if the peak never budges.
    patchCables_.emplace_back(std::make_unique<AudioConnection>(voiceMixerLeft_[1], 0, voiceFanoutProbe_, 0));
  }
#endif
}

void ResonatorBank::prepare(const Engine::PrepareContext& ctx) {
  init(ctx.hardware ? Mode::kHardware : Mode::kSim);
  (void)ctx.masterSeed;
  (void)ctx.sampleRate;
  (void)ctx.framesPerBlock;
}

void ResonatorBank::onTick(const Engine::TickContext& ctx) {
  (void)ctx;
}

void ResonatorBank::onParam(const Engine::ParamChange& change) {
  (void)change;
}

void ResonatorBank::onSeed(const Engine::SeedContext& ctx) {
  trigger(ctx.seed, ctx.whenSamples);
}

void ResonatorBank::renderAudio(const Engine::RenderContext& ctx) {
  (void)ctx;
}

Engine::StateBuffer ResonatorBank::serializeState() const {
  return {};
}

void ResonatorBank::deserializeState(const Engine::StateBuffer& state) {
  (void)state;
}

void ResonatorBank::setMaxVoices(uint8_t voices) {
  maxVoices_ = std::max<uint8_t>(1, std::min<uint8_t>(voices, kMaxVoices));
}

void ResonatorBank::setDampingRange(float minDamping, float maxDamping) {
  minDamping_ = std::min(minDamping, maxDamping);
  maxDamping_ = std::max(minDamping, maxDamping);
}

uint8_t ResonatorBank::activeVoices() const {
  return static_cast<uint8_t>(std::count_if(voices_.begin(), voices_.end(), [](const VoiceInternal& v) {
    return v.active;
  }));
}

uint8_t ResonatorBank::allocateVoice() {
  for (uint8_t i = 0; i < maxVoices_; ++i) {
    if (!voices_[i].active) {
      return i;
    }
  }
  // steal the oldest voice to keep the modal bank ringing
  uint8_t oldest = 0;
  uint32_t minSample = voices_[0].startSample;
  uint32_t minHandle = voices_[0].handle;
  for (uint8_t i = 1; i < maxVoices_; ++i) {
    if (voices_[i].startSample < minSample ||
        (voices_[i].startSample == minSample && voices_[i].handle < minHandle)) {
      oldest = i;
      minSample = voices_[i].startSample;
      minHandle = voices_[i].handle;
    }
  }
  return oldest;
}

void ResonatorBank::planExcitation(VoiceInternal& v, const Seed& seed, uint32_t whenSamples) {
  v.active = true;
  v.startSample = whenSamples;
  v.seedId = static_cast<uint8_t>(seed.id & 0xFF);
  v.mode = clampMode(seed.resonator.mode);
  v.bank = (seed.resonator.bank < presets_.size()) ? seed.resonator.bank : static_cast<uint8_t>(presets_.size() - 1);
  v.preset = &resolvePreset(v.bank);

  v.handle = nextHandle_++;
  if (nextHandle_ == 0) {
    nextHandle_ = 1;
  }

  const float semitones = seed.pitch;
  const float baseHz = 110.0f; // A2 reference
  v.frequency = baseHz * std::pow(2.0f, semitones / 12.0f);

  v.burstMs = std::max(0.25f, seed.resonator.exciteMs);

  const float dampNorm = clamp01(seed.resonator.damping);
  v.damping = lerp(minDamping_, maxDamping_, dampNorm);

  const float seedBrightness = clamp01(seed.resonator.brightness);
  v.brightness = clamp01(lerp(v.preset->baseBrightness, seedBrightness, 0.7f));

  const float seedFeedback = clamp01(seed.resonator.feedback);
  v.feedback = clamp01(lerp(v.preset->baseFeedback, seedFeedback, 0.65f));

  v.delaySamples = std::max(1.0f, Units::kSampleRate / std::max(10.0f, v.frequency));

  // Calculate burst gain so brighter hits lean into the modal bank harder while
  // still respecting damping — darker hits sound softer.
  const float dampingComp = 1.0f - (v.damping - minDamping_) / std::max(0.0001f, (maxDamping_ - minDamping_));
  v.burstGain = lerp(0.45f, 1.25f, v.brightness) * lerp(0.5f, 1.0f, dampingComp);

  for (uint8_t i = 0; i < v.modalFrequencies.size(); ++i) {
    const float ratio = v.preset->modeRatios[i];
    const float presetGain = v.preset->modeGains[i];
    v.modalFrequencies[i] = v.frequency * ratio;
    const float emphasis = lerp(0.6f, 1.4f, v.brightness) * (1.0f - 0.1f * i);
    v.modalGains[i] = clamp01(presetGain * emphasis);
  }

  // Kick RNG to prep for future burst-shaping randomness — we stash nothing yet
  // but advance the generator so tests stay deterministic once jitter arrives.
  uint32_t prng = seed.prng;
  const float exciteScatter = RNG::uniform01(prng);
  (void)exciteScatter;
}

void ResonatorBank::trigger(const Seed& seed, uint32_t whenSamples) {
  if (maxVoices_ == 0) {
    return;
  }
  const uint8_t voiceIndex = allocateVoice();
  VoiceInternal& voiceSlot = voices_[voiceIndex];

  planExcitation(voiceSlot, seed, whenSamples);
  // Once the plan is ready, map it onto either the Teensy audio nodes or the
  // simulator mirrors so downstream tests can inspect state.
  mapVoiceToGraph(voiceIndex, voiceSlot);
}

const char* ResonatorBank::presetName(uint8_t bank) const {
  if (bank >= presets_.size()) {
    return presets_.back().name;
  }
  return presets_[bank].name;
}

ResonatorBank::VoiceState ResonatorBank::voice(uint8_t voiceIndex) const {
  VoiceState out;
  if (voiceIndex >= kMaxVoices) {
    return out;
  }

  const VoiceInternal& src = voices_[voiceIndex];
  out.active = src.active;
  out.handle = src.handle;
  out.startSample = src.startSample;
  out.seedId = src.seedId;
  out.frequency = src.frequency;
  out.burstMs = src.burstMs;
  out.damping = src.damping;
  out.brightness = src.brightness;
  out.feedback = src.feedback;
  out.burstGain = src.burstGain;
  out.delaySamples = src.delaySamples;
  out.modalFrequencies = src.modalFrequencies;
  out.modalGains = src.modalGains;
  out.mode = src.mode;
  out.bank = src.bank;
  out.preset = src.preset ? src.preset->name : nullptr;
  return out;
}

void ResonatorBank::mapVoiceToGraph(uint8_t voiceIndex, VoiceInternal& voicePlan) {
#ifdef SEEDBOX_HW
  auto &hwVoice = hwVoices_[voiceIndex];

  hwVoice.burstEnv.attack(voicePlan.burstMs);
  hwVoice.burstEnv.decay(std::max(1.0f, voicePlan.burstMs * 0.5f));
  hwVoice.burstEnv.sustain(0.0f);
  hwVoice.burstEnv.release(std::max(1.0f, voicePlan.burstMs));

  const float tiltHz = lerp(700.0f, 6000.0f, voicePlan.brightness);
  hwVoice.brightnessFilter.frequency(tiltHz);
  hwVoice.brightnessFilter.resonance(0.7f + 0.2f * (1.0f - voicePlan.damping));

  const float delayMs = (voicePlan.delaySamples / Units::kSampleRate) * 1000.0f;
  hwVoice.stringDelay.delay(0, std::max(0.1f, delayMs));

  for (uint8_t m = 0; m < hwVoice.modalFilters.size(); ++m) {
    const float freq = std::max(40.0f, voicePlan.modalFrequencies[m]);
    hwVoice.modalFilters[m].setBandpass(0, freq, 1.2f);
    hwVoice.modalMix.gain(m, voicePlan.modalGains[m]);
  }

  const float sustainGain = lerp(0.1f, 0.95f, voicePlan.feedback);
  hwVoice.mix.gain(0, sustainGain);
  hwVoice.mix.gain(1, lerp(0.4f, 1.0f, voicePlan.brightness));

  const float left = std::sqrt(0.5f);
  const float right = std::sqrt(0.5f);
  const uint8_t group = static_cast<uint8_t>(voiceIndex / kMixerFanIn);
  const uint8_t slot = static_cast<uint8_t>(voiceIndex % kMixerFanIn);
  voiceMixerLeft_[group].gain(slot, left * voicePlan.burstGain);
  voiceMixerRight_[group].gain(slot, right * voicePlan.burstGain);

  hwVoice.burstNoise.amplitude(voicePlan.burstGain);
  hwVoice.burstEnv.noteOn();
#else
  (void)voiceIndex;
  (void)voicePlan;
#endif
}

#ifdef SEEDBOX_HW
float ResonatorBank::fanoutProbeLevel() const {
  if (voiceFanoutProbe_.available()) {
    return voiceFanoutProbe_.read();
  }
  return 0.0f;
}
#endif

const ResonatorBank::ModalPreset& ResonatorBank::resolvePreset(uint8_t bank) const {
  if (bank >= presets_.size()) {
    return presets_.back();
  }
  return presets_[bank];
}

uint8_t ResonatorBank::clampMode(uint8_t requested) const {
  return requested == 0 ? 0 : 1;
}

void ResonatorBank::onSeed(const Seed& seed) {
  const std::size_t index = static_cast<std::size_t>(seed.id);
  if (seedCache_.size() <= index) {
    seedCache_.resize(index + 1);
  }
  seedCache_[index] = seed;
}

const Seed* ResonatorBank::lastSeed(uint32_t id) const {
  const std::size_t index = static_cast<std::size_t>(id);
  if (index >= seedCache_.size()) {
    return nullptr;
  }
  return &seedCache_[index];
}

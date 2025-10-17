#include "engine/Resonator.h"
#include <algorithm>
#include <array>
#include <cmath>
#include "util/RNG.h"
#include "util/Units.h"

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

  for (auto &v : voices_) {
    v = VoiceInternal{};
  }

#ifdef SEEDBOX_HW
  patchCables_.clear();

  for (uint8_t i = 0; i < kMaxVoices; ++i) {
    auto &hw = hwVoices_[i];

    hw.burstEnv.attack(0.25f);
    hw.burstEnv.decay(3.0f);
    hw.burstEnv.sustain(0.0f);
    hw.burstEnv.release(2.0f);

    hw.brightnessFilter.frequency(4000.0f);
    hw.brightnessFilter.resonance(0.7f);

    hw.stringDelay.delay(0, 10.0f);

    for (uint8_t m = 0; m < hw.modalFilters.size(); ++m) {
      hw.modalFilters[m].setBandpass(0, 800.0f + 200.0f * m, 1.4f);
      hw.modalMix.gain(m, 0.0f);
    }

    hw.mix.gain(0, 0.0f);
    hw.mix.gain(1, 0.0f);

    patchCables_.emplace_back(std::make_unique<AudioConnection>(hw.burstNoise, 0, hw.burstEnv, 0));
    patchCables_.emplace_back(std::make_unique<AudioConnection>(hw.burstEnv, 0, hw.brightnessFilter, 0));
    patchCables_.emplace_back(std::make_unique<AudioConnection>(hw.brightnessFilter, 0, hw.stringDelay, 0));
    patchCables_.emplace_back(std::make_unique<AudioConnection>(hw.stringDelay, 0, hw.mix, 0));

    for (uint8_t m = 0; m < hw.modalFilters.size(); ++m) {
      patchCables_.emplace_back(std::make_unique<AudioConnection>(hw.brightnessFilter, 0, hw.modalFilters[m], 0));
      patchCables_.emplace_back(std::make_unique<AudioConnection>(hw.modalFilters[m], 0, hw.modalMix, m));
    }

    patchCables_.emplace_back(std::make_unique<AudioConnection>(hw.modalMix, 0, hw.mix, 1));

    patchCables_.emplace_back(std::make_unique<AudioConnection>(hw.mix, 0, voiceMixerLeft_, i));
    patchCables_.emplace_back(std::make_unique<AudioConnection>(hw.mix, 0, voiceMixerRight_, i));

    voiceMixerLeft_.gain(i, 0.0f);
    voiceMixerRight_.gain(i, 0.0f);
  }

  patchCables_.emplace_back(std::make_unique<AudioConnection>(voiceMixerLeft_, 0, output_, 0));
  patchCables_.emplace_back(std::make_unique<AudioConnection>(voiceMixerRight_, 0, output_, 1));
#endif
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
  const uint8_t index = allocateVoice();
  VoiceInternal& voice = voices_[index];

  planExcitation(voice, seed, whenSamples);
  mapVoiceToGraph(index, voice);
}

const char* ResonatorBank::presetName(uint8_t bank) const {
  if (bank >= presets_.size()) {
    return presets_.back().name;
  }
  return presets_[bank].name;
}

ResonatorBank::VoiceState ResonatorBank::voice(uint8_t index) const {
  VoiceState out;
  if (index >= kMaxVoices) {
    return out;
  }

  const VoiceInternal& src = voices_[index];
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

void ResonatorBank::mapVoiceToGraph(uint8_t index, VoiceInternal& voice) {
#ifdef SEEDBOX_HW
  auto &hw = hwVoices_[index];

  hw.burstEnv.attack(voice.burstMs);
  hw.burstEnv.decay(std::max(1.0f, voice.burstMs * 0.5f));
  hw.burstEnv.sustain(0.0f);
  hw.burstEnv.release(std::max(1.0f, voice.burstMs));

  const float tiltHz = lerp(700.0f, 6000.0f, voice.brightness);
  hw.brightnessFilter.frequency(tiltHz);
  hw.brightnessFilter.resonance(0.7f + 0.2f * (1.0f - voice.damping));

  const float delayMs = (voice.delaySamples / Units::kSampleRate) * 1000.0f;
  hw.stringDelay.delay(0, std::max(0.1f, delayMs));

  for (uint8_t m = 0; m < hw.modalFilters.size(); ++m) {
    const float freq = std::max(40.0f, voice.modalFrequencies[m]);
    hw.modalFilters[m].setBandpass(0, freq, 1.2f);
    hw.modalMix.gain(m, voice.modalGains[m]);
  }

  const float sustainGain = lerp(0.1f, 0.95f, voice.feedback);
  hw.mix.gain(0, sustainGain);
  hw.mix.gain(1, lerp(0.4f, 1.0f, voice.brightness));

  const float left = std::sqrt(0.5f);
  const float right = std::sqrt(0.5f);
  voiceMixerLeft_.gain(index, left * voice.burstGain);
  voiceMixerRight_.gain(index, right * voice.burstGain);

  hw.burstNoise.amplitude(voice.burstGain);
  hw.burstEnv.noteOn();
#else
  (void)index;
  (void)voice;
#endif
}

const ResonatorBank::ModalPreset& ResonatorBank::resolvePreset(uint8_t bank) const {
  if (bank >= presets_.size()) {
    return presets_.back();
  }
  return presets_[bank];
}

uint8_t ResonatorBank::clampMode(uint8_t requested) const {
  return requested == 0 ? 0 : 1;
}

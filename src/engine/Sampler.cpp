#include "engine/Sampler.h"
#include <algorithm>
#include <array>
#include <cmath>
#ifdef SEEDBOX_HW
#include <Arduino.h>
#include <Audio.h>
#include <memory>
#endif

namespace {
// How many samples we expect to have preloaded into flash/RAM. Once a seed asks
// for a sample index equal or above this, we flip the voice over to SD card
// streaming. Right now we just match the voice count so the tests can exercise
// both paths.
constexpr uint8_t kRamPreloadCount = Sampler::kMaxVoices;
constexpr float kHalfPi = 1.57079637f;

#ifdef SEEDBOX_HW
constexpr float msFromSeconds(float seconds) { return seconds * 1000.0f; }
#endif
}

Sampler::Sampler() = default;

Sampler::~Sampler() = default;

#ifdef SEEDBOX_HW
struct Sampler::HardwareVoice {
  AudioPlayMemory ramPlayer;
  AudioPlaySdWav sdPlayer;
  AudioMixer4 sourceMixer;
  AudioEffectEnvelope envelope;
  AudioFilterStateVariable toneFilter;
};

struct Sampler::HardwareGraph {
  AudioMixer4 voiceMixerLeft;
  AudioMixer4 voiceMixerRight;
  AudioOutputI2S output;
  std::array<std::unique_ptr<HardwareVoice>, Sampler::kMaxVoices> voices;
  std::vector<std::unique_ptr<AudioConnection>> patchCables;
  bool wired{false};

  HardwareGraph() {
    patchCables.reserve(static_cast<size_t>(Sampler::kMaxVoices) * 6 + 2);
  }
};
#endif

void Sampler::init() {
  nextHandle_ = 1;
  for (auto& v : voices_) {
    // Reset every voice slot back to a blank template. This mirrors what
    // happens when you power-cycle the synth — no lingering envelope states or
    // stale sample handles survive a call to `init`.
    v = VoiceInternal{};
  }

#ifdef SEEDBOX_HW
  ensureHardwareGraph();
#else
  // Native build keeps the data-path only. Audio nodes live exclusively in the
  // hardware target to avoid dragging Teensy headers into tests.
#endif
}

#ifdef SEEDBOX_HW
void Sampler::ensureHardwareGraph() {
  if (!hardware_) {
    hardware_ = std::make_unique<HardwareGraph>();
  }

  auto& hw = *hardware_;
  if (hw.wired) {
    return;
  }

  hw.patchCables.clear();

  // Reserve audio memory blocks for the voice pool. Teensy Audio needs to know
  // upfront how many 128-sample buffers it can juggle. This is the knob to
  // reach for if notes start crackling once we stream from SD.
  AudioMemory(96);

  for (uint8_t i = 0; i < kMaxVoices; ++i) {
    if (!hw.voices[i]) {
      hw.voices[i] = std::make_unique<HardwareVoice>();
    }
    auto& voice = *hw.voices[i];

    // Default mixer gains balance the RAM + SD sources. Later, once actual
    // sample tables land, a trigger call toggles the appropriate source on/off.
    voice.sourceMixer.gain(0, 1.0f);
    voice.sourceMixer.gain(1, 1.0f);

    // Prime the envelope with sane defaults. The configureVoice path overwrites
    // these values every trigger so the sampler tracks Seed intent.
    voice.envelope.attack(1.0f);
    voice.envelope.decay(50.0f);
    voice.envelope.sustain(0.8f);
    voice.envelope.release(100.0f);

    // Tone filter starts bright-ish and wide. ConfigureVoice narrows it based on
    // the seed's tilt request.
    voice.toneFilter.frequency(6000.0f);
    voice.toneFilter.resonance(0.707f);

    // Patch the signal chain:
    //   RAM sample + SD sample -> mixer -> envelope -> tilt filter -> stereo mix
    hw.patchCables.emplace_back(std::make_unique<AudioConnection>(voice.ramPlayer, 0, voice.sourceMixer, 0));
    hw.patchCables.emplace_back(std::make_unique<AudioConnection>(voice.sdPlayer, 0, voice.sourceMixer, 1));
    hw.patchCables.emplace_back(std::make_unique<AudioConnection>(voice.sourceMixer, 0, voice.envelope, 0));
    hw.patchCables.emplace_back(std::make_unique<AudioConnection>(voice.envelope, 0, voice.toneFilter, 0));
    hw.patchCables.emplace_back(std::make_unique<AudioConnection>(voice.toneFilter, 0, hw.voiceMixerLeft, i));
    hw.patchCables.emplace_back(std::make_unique<AudioConnection>(voice.toneFilter, 0, hw.voiceMixerRight, i));

    hw.voiceMixerLeft.gain(i, 0.0f);
    hw.voiceMixerRight.gain(i, 0.0f);
  }

  hw.patchCables.emplace_back(std::make_unique<AudioConnection>(hw.voiceMixerLeft, 0, hw.output, 0));
  hw.patchCables.emplace_back(std::make_unique<AudioConnection>(hw.voiceMixerRight, 0, hw.output, 1));

  hw.wired = true;
}
#endif

uint8_t Sampler::activeVoices() const {
  return static_cast<uint8_t>(std::count_if(voices_.begin(), voices_.end(), [](const VoiceInternal& v) {
    return v.active;
  }));
}

Sampler::VoiceState Sampler::voice(uint8_t index) const {
  VoiceState state;
  if (index >= kMaxVoices) {
    return state;
  }

  const VoiceInternal& src = voices_[index];
  state.active = src.active;
  state.handle = src.handle;
  state.startSample = src.startSample;
  state.sampleIndex = src.sampleIndex;
  state.playbackRate = src.playbackRate;
  state.envelope.attack = src.envA;
  state.envelope.decay = src.envD;
  state.envelope.sustain = src.envS;
  state.envelope.release = src.envR;
  state.tone = src.tone;
  state.spread = src.spread;
  state.usesSdStreaming = src.usesSdStreaming;
  state.leftGain = src.leftGain;
  state.rightGain = src.rightGain;
  return state;
}

uint8_t Sampler::allocateVoice(uint32_t /*whenSamples*/) {
  // First pass: look for a free slot. Most of the time we will find one because
  // the pool is tiny and percussive.
  for (uint8_t i = 0; i < kMaxVoices; ++i) {
    if (!voices_[i].active) {
      return i;
    }
  }

  // No free slots: steal the oldest voice (FIFO). The `startSample` clock wins;
  // if two triggers landed at the same sample, the lower handle (older trigger)
  // gets the boot.
  uint8_t oldest = 0;
  uint32_t oldestStart = voices_[0].startSample;
  uint32_t oldestHandle = voices_[0].handle;
  for (uint8_t i = 1; i < kMaxVoices; ++i) {
    if (voices_[i].startSample < oldestStart ||
        (voices_[i].startSample == oldestStart && voices_[i].handle < oldestHandle)) {
      oldest = i;
      oldestStart = voices_[i].startSample;
      oldestHandle = voices_[i].handle;
    }
  }

  return oldest;
}

void Sampler::configureVoice(VoiceInternal& voice, uint8_t index, const Seed& seed, uint32_t whenSamples) {
#ifndef SEEDBOX_HW
  (void)index;
#endif
  // Mark the slot active and capture every seed parameter so state inspection is
  // just reading from this struct.
  voice.active = true;
  voice.startSample = whenSamples;
  voice.sampleIndex = seed.sampleIdx;
  voice.playbackRate = pitchToPlaybackRate(seed.pitch);
  voice.envA = std::max(0.0f, seed.envA);
  voice.envD = std::max(0.0f, seed.envD);
  voice.envS = clamp01(seed.envS);
  voice.envR = std::max(0.0f, seed.envR);
  voice.tone = clamp01(seed.tone);
  voice.spread = clamp01(seed.spread);
  voice.usesSdStreaming = (seed.sampleIdx >= kRamPreloadCount);

  // Constant-power pan law: treat spread as a normalized left/right pan, map it
  // onto a quarter circle so the combined power stays roughly constant.
  const float pan = (voice.spread * 2.0f) - 1.0f;
  const float angle = (pan + 1.0f) * 0.5f * kHalfPi;
  voice.leftGain = std::cos(angle);
  voice.rightGain = std::sin(angle);

#ifdef SEEDBOX_HW
  ensureHardwareGraph();
  auto& hw = *hardware_->voices[index];
  if (voice.usesSdStreaming) {
    hw.sourceMixer.gain(0, 0.0f);
    hw.sourceMixer.gain(1, 1.0f);
  } else {
    hw.sourceMixer.gain(0, 1.0f);
    hw.sourceMixer.gain(1, 0.0f);
  }

  // Translate envelope times into milliseconds for the Teensy ADSR block.
  hw.envelope.attack(msFromSeconds(voice.envA));
  hw.envelope.decay(msFromSeconds(voice.envD));
  hw.envelope.sustain(voice.envS);
  hw.envelope.release(msFromSeconds(voice.envR));

  // Map the 0-1 tone knob to a useful musical range.
  const float minTiltHz = 400.0f;
  const float maxTiltHz = 8000.0f;
  const float freq = minTiltHz + (maxTiltHz - minTiltHz) * voice.tone;
  hw.toneFilter.frequency(freq);
  hw.toneFilter.resonance(0.707f);

  // Stereo gains feed into a pair of mixers that eventually land on I²S out.
  hardware_->voiceMixerLeft.gain(index, voice.leftGain);
  hardware_->voiceMixerRight.gain(index, voice.rightGain);

  // Placeholder scheduling: AudioPlayMemory/SdWav start calls will land here
  // once sample assets are wired. For now we rely on startSample to keep the
  // trigger deterministic for future DSP hookup.
  hw.envelope.noteOn();
#endif
}

float Sampler::pitchToPlaybackRate(float semitones) {
  return std::pow(2.0f, semitones / 12.0f);
}

float Sampler::clamp01(float value) {
  return std::max(0.0f, std::min(1.0f, value));
}

void Sampler::trigger(const Seed& seed, uint32_t whenSamples) {
#ifdef SEEDBOX_HW
  ensureHardwareGraph();
#endif
  const uint8_t index = allocateVoice(whenSamples);
  VoiceInternal& voice = voices_[index];

  // Increment the deterministic handle. Wrap around safely (0 is reserved so we
  // can treat it as "uninitialized").
  voice.handle = nextHandle_++;
  if (nextHandle_ == 0) {
    nextHandle_ = 1;
  }

  // Bake every seed attribute into the voice record + hardware nodes.
  configureVoice(voice, index, seed, whenSamples);
}

//
// Sampler.cpp
// -----------
// Implementation notes that compliment the header.  We keep the DSP placeholders
// lightweight but loud — students can read through this file and understand how
// a voice allocator, envelope setup, and constant-power panning behave before we
// ever hook up real sample data.
#include "engine/Sampler.h"

#include <algorithm>
#include <cmath>

#include "engine/Stereo.h"

namespace {
// How many samples we expect to have preloaded into flash/RAM. Once a seed asks
// for a sample index equal or above this, we flip the voice over to SD card
// streaming. Right now we just match the voice count so the tests can exercise
// both paths.
constexpr uint8_t kRamPreloadCount = Sampler::kMaxVoices;
#ifdef SEEDBOX_HW
constexpr float msFromSeconds(float seconds) { return seconds * 1000.0f; }
#endif
}

void Sampler::init() {
  nextHandle_ = 1;
  // Reset every voice slot back to a blank template. This mirrors what happens
  // when you power-cycle the synth — no lingering envelope states or stale
  // sample handles survive a call to `init`.
  voices_.fill(VoiceInternal{});

#ifdef SEEDBOX_HW
  patchCables_.clear();

  for (uint8_t i = 0; i < kMaxVoices; ++i) {
    auto& hwVoice = hwVoices_[i];

    // Default mixer gains balance the RAM + SD sources. Later, once actual
    // sample tables land, a trigger call toggles the appropriate source on/off.
    hwVoice.sourceMixer.gain(0, 1.0f);
    hwVoice.sourceMixer.gain(1, 1.0f);

    // Prime the envelope with sane defaults. The configureVoice path overwrites
    // these values every trigger so the sampler tracks Seed intent.
    hwVoice.envelope.attack(1.0f);
    hwVoice.envelope.decay(50.0f);
    hwVoice.envelope.sustain(0.8f);
    hwVoice.envelope.release(100.0f);

    // Tone filter starts bright-ish and wide. ConfigureVoice narrows it based on
    // the seed's tilt request.
    hwVoice.toneFilter.frequency(6000.0f);
    hwVoice.toneFilter.resonance(0.707f);

    // Patch the signal chain:
    //   RAM sample + SD sample -> mixer -> envelope -> tilt filter -> stereo mix
    patchCables_.emplace_back(std::make_unique<AudioConnection>(hwVoice.ramPlayer, 0, hwVoice.sourceMixer, 0));
    patchCables_.emplace_back(std::make_unique<AudioConnection>(hwVoice.sdPlayer, 0, hwVoice.sourceMixer, 1));
    patchCables_.emplace_back(std::make_unique<AudioConnection>(hwVoice.sourceMixer, 0, hwVoice.envelope, 0));
    patchCables_.emplace_back(std::make_unique<AudioConnection>(hwVoice.envelope, 0, hwVoice.toneFilter, 0));
    patchCables_.emplace_back(std::make_unique<AudioConnection>(hwVoice.toneFilter, 0, voiceMixerLeft_, i));
    patchCables_.emplace_back(std::make_unique<AudioConnection>(hwVoice.toneFilter, 0, voiceMixerRight_, i));

    voiceMixerLeft_.gain(i, 0.0f);
    voiceMixerRight_.gain(i, 0.0f);
  }

  patchCables_.emplace_back(std::make_unique<AudioConnection>(voiceMixerLeft_, 0, output_, 0));
  patchCables_.emplace_back(std::make_unique<AudioConnection>(voiceMixerRight_, 0, output_, 1));
#else
  // Native build keeps the data-path only. Audio nodes live exclusively in the
  // hardware target to avoid dragging Teensy headers into tests.
#endif
}

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

uint8_t Sampler::allocateVoice(uint32_t whenSamples) {
  (void)whenSamples;
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

  // Constant-power width law: `spread` 0 => centered, 1 => hard pan. Future
  // versions can feed a polarity flag to swing left; for now we focus on the
  // "mono to wide" journey students can hear immediately.
  const auto gains = stereo::constantPowerWidth(voice.spread);
  voice.leftGain = gains.left;
  voice.rightGain = gains.right;

#ifdef SEEDBOX_HW
  auto& hw = hwVoices_[index];
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
  voiceMixerLeft_.gain(index, voice.leftGain);
  voiceMixerRight_.gain(index, voice.rightGain);

  // Placeholder scheduling: AudioPlayMemory/SdWav start calls will land here
  // once sample assets are wired. For now we rely on startSample to keep the
  // trigger deterministic for future DSP hookup.
  hw.envelope.noteOn();
#else
  (void)index;
#endif
}

float Sampler::pitchToPlaybackRate(float semitones) {
  return std::pow(2.0f, semitones / 12.0f);
}

float Sampler::clamp01(float value) {
  return std::max(0.0f, std::min(1.0f, value));
}

void Sampler::onSeed(const Seed& seed) {
  const std::size_t index = static_cast<std::size_t>(seed.id);
  if (seedCache_.size() <= index) {
    seedCache_.resize(index + 1);
  }
  seedCache_[index] = seed;
}

const Seed* Sampler::lastSeed(uint32_t id) const {
  const std::size_t index = static_cast<std::size_t>(id);
  if (index >= seedCache_.size()) {
    return nullptr;
  }
  return &seedCache_[index];
}

void Sampler::trigger(const Seed& seed, uint32_t whenSamples) {
  const uint8_t voiceIndex = allocateVoice(whenSamples);
  VoiceInternal& voiceSlot = voices_[voiceIndex];

  // Increment the deterministic handle. Wrap around safely (0 is reserved so we
  // can treat it as "uninitialized").
  voiceSlot.handle = nextHandle_++;
  if (nextHandle_ == 0) {
    nextHandle_ = 1;
  }

  // Bake every seed attribute into the voice record + hardware nodes.
  configureVoice(voiceSlot, voiceIndex, seed, whenSamples);
}

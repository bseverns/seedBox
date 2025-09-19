#include "engine/Resonator.h"
#include <algorithm>
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
}

void ResonatorBank::init(Mode mode) {
  mode_ = mode;
  maxVoices_ = (mode == Mode::kHardware) ? 10 : 4;
  for (auto &v : voices_) {
    v = Voice{};
  }
}

void ResonatorBank::setMaxVoices(uint8_t voices) {
  maxVoices_ = std::max<uint8_t>(1, std::min<uint8_t>(voices, kMaxVoices));
}

void ResonatorBank::setDampingRange(float minDamping, float maxDamping) {
  minDamping_ = std::min(minDamping, maxDamping);
  maxDamping_ = std::max(minDamping, maxDamping);
}

uint8_t ResonatorBank::activeVoices() const {
  return static_cast<uint8_t>(std::count_if(voices_.begin(), voices_.end(), [](const Voice& v) {
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
  for (uint8_t i = 1; i < maxVoices_; ++i) {
    if (voices_[i].startSample < minSample) {
      oldest = i;
      minSample = voices_[i].startSample;
    }
  }
  return oldest;
}

void ResonatorBank::planExcitation(Voice& v, const Seed& seed, uint32_t whenSamples) {
  v.active = true;
  v.startSample = whenSamples;
  v.seedId = static_cast<uint8_t>(seed.id);
  v.mode = seed.resonator.mode;
  v.bank = seed.resonator.bank;

  const float semitones = seed.pitch;
  const float baseHz = 110.0f; // A2 reference
  v.frequency = baseHz * std::pow(2.0f, semitones / 12.0f);

  const float dampNorm = clamp01(seed.resonator.damping);
  v.damping = lerp(minDamping_, maxDamping_, dampNorm);
  v.brightness = clamp01(seed.resonator.brightness);
  v.feedback = clamp01(seed.resonator.feedback);

  // Kick RNG to prep for future burst-shaping randomness.
  uint32_t prng = seed.prng;
  const float exciteScatter = RNG::uniform01(prng);
  (void)exciteScatter;

  // When we wire in the DSP graph we'll convert seed.resonator.exciteMs into a
  // burst envelope duration. Units::msToSamples keeps the conversion consistent
  // between native and hardware builds, so the control path is ready now.
  const uint32_t exciteSamples = Units::msToSamples(seed.resonator.exciteMs);
  (void)exciteSamples;
}

void ResonatorBank::trigger(const Seed& seed, uint32_t whenSamples) {
  if (maxVoices_ == 0) {
    return;
  }
  const uint8_t index = allocateVoice();
  planExcitation(voices_[index], seed, whenSamples);

  // DSP hookup will land here: allocate a short excitation burst, feed it into
  // the Karplus-Strong string or modal resonator object, then let the Teensy
  // Audio library do the heavy lifting.
}

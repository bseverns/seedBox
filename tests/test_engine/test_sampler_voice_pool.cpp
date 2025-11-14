#include <array>
#include <cmath>
#include <unity.h>

#include "Seed.h"
#include "engine/Sampler.h"

namespace {
// Helper factory so each test can stamp out a Seed with predictable values.
Seed makeSeed(uint32_t id, uint8_t sampleIdx, float pitch, float tone, float spread) {
  Seed seed;
  seed.id = id;
  seed.sampleIdx = sampleIdx;
  seed.pitch = pitch;
  seed.envA = 0.005f;
  seed.envD = 0.12f;
  seed.envS = 0.66f;
  seed.envR = 0.2f;
  seed.tone = tone;
  seed.spread = spread;
  return seed;
}
}

void test_sampler_stores_seed_state() {
  Sampler sampler;
  sampler.init();

  // Pick a seed that forces the sampler to think "stream from SD" and use a
  // non-zero launch time so we exercise every field in VoiceState.
  const Seed seed = makeSeed(1, Sampler::kMaxVoices + 2, 7.0f, 0.8f, 0.65f);
  const uint32_t whenSamples = 4800;
  sampler.trigger(seed, whenSamples);

  const Sampler::VoiceState state = sampler.voice(0);
  TEST_ASSERT_TRUE(state.active);
  TEST_ASSERT_EQUAL_UINT32(whenSamples, state.startSample);
  TEST_ASSERT_EQUAL_UINT8(seed.sampleIdx, state.sampleIndex);
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, std::pow(2.0f, seed.pitch / 12.0f), state.playbackRate);
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, seed.envA, state.envelope.attack);
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, seed.envD, state.envelope.decay);
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, seed.envS, state.envelope.sustain);
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, seed.envR, state.envelope.release);
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, seed.tone, state.tone);
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, seed.spread, state.spread);
  TEST_ASSERT_TRUE(state.usesSdStreaming);
  TEST_ASSERT_TRUE(state.leftGain >= 0.0f);
  TEST_ASSERT_TRUE(state.rightGain >= 0.0f);
}

void test_sampler_voice_stealing_is_oldest_first() {
  Sampler sampler;
  sampler.init();

  // Fill every voice slot with unique parameters so we can tell which voice is
  // which after a steal.
  std::array<Sampler::VoiceState, Sampler::kMaxVoices> before{};
  const uint32_t baseWhen = 1600;
  for (uint8_t i = 0; i < Sampler::kMaxVoices; ++i) {
    const Seed seed = makeSeed(i + 1, i, static_cast<float>(i), 0.2f + 0.1f * i, 0.15f + 0.1f * i);
    sampler.trigger(seed, baseWhen + static_cast<uint32_t>(i) * 240);
    before[i] = sampler.voice(i);
  }

  // Manually compute which voice *should* be stolen: the one with the earliest
  // start sample, breaking ties with the lowest handle.
  uint8_t expectedSteal = 0;
  uint32_t earliestStart = before[0].startSample;
  uint32_t earliestHandle = before[0].handle;
  uint32_t maxHandleBefore = before[0].handle;
  for (uint8_t i = 1; i < Sampler::kMaxVoices; ++i) {
    if (before[i].startSample < earliestStart ||
        (before[i].startSample == earliestStart && before[i].handle < earliestHandle)) {
      expectedSteal = i;
      earliestStart = before[i].startSample;
      earliestHandle = before[i].handle;
    }
    if (before[i].handle > maxHandleBefore) {
      maxHandleBefore = before[i].handle;
    }
  }

  const Seed overflow = makeSeed(99, Sampler::kMaxVoices + 5, 3.5f, 0.6f, 0.9f);
  const uint32_t newWhen = baseWhen + static_cast<uint32_t>(Sampler::kMaxVoices) * 240;
  sampler.trigger(overflow, newWhen);

  TEST_ASSERT_EQUAL_UINT8(Sampler::kMaxVoices, sampler.activeVoices());

  uint8_t foundIndex = Sampler::kMaxVoices;
  for (uint8_t i = 0; i < Sampler::kMaxVoices; ++i) {
    const auto state = sampler.voice(i);
    if (state.sampleIndex == overflow.sampleIdx) {
      foundIndex = i;
      TEST_ASSERT_EQUAL_UINT32(newWhen, state.startSample);
      TEST_ASSERT_TRUE(state.handle > maxHandleBefore);
    } else {
      // All other slots should retain their original handles, proving we only
      // kicked out one voice.
      TEST_ASSERT_EQUAL_UINT32(before[i].handle, state.handle);
    }
  }

  TEST_ASSERT_NOT_EQUAL(Sampler::kMaxVoices, foundIndex);
  TEST_ASSERT_EQUAL_UINT8(expectedSteal, foundIndex);
}

void test_sampler_spread_width_maps_constant_power_curve() {
  Sampler sampler;
  sampler.init();

  const Seed centered = makeSeed(11, 0, 0.0f, 0.3f, 0.0f);
  sampler.trigger(centered, 0u);
  auto state = sampler.voice(0);
  TEST_ASSERT_TRUE(state.active);
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, state.leftGain, state.rightGain);
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.70710677f, state.leftGain);

  sampler.init();
  const Seed wide = makeSeed(12, 1, 0.0f, 0.3f, 1.0f);
  sampler.trigger(wide, 0u);
  state = sampler.voice(0);
  TEST_ASSERT_TRUE(state.active);
  TEST_ASSERT_TRUE(state.rightGain > state.leftGain);
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, state.leftGain);
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.0f, state.rightGain);
}

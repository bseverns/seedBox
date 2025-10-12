#include <algorithm>
#include <array>
#include <cmath>
#include <unity.h>

#include "Seed.h"
#include "engine/Resonator.h"
#include "util/Units.h"

namespace {
Seed makeSeed(uint8_t id,
              float pitch,
              float exciteMs,
              float damping,
              float brightness,
              float feedback,
              uint8_t mode,
              uint8_t bank) {
  Seed seed{};
  seed.id = id;
  seed.prng = 0x12345678u + id;  // deterministic but unique per voice.
  seed.pitch = pitch;
  seed.resonator.exciteMs = exciteMs;
  seed.resonator.damping = damping;
  seed.resonator.brightness = brightness;
  seed.resonator.feedback = feedback;
  seed.resonator.mode = mode;
  seed.resonator.bank = bank;
  return seed;
}
}

void test_resonator_maps_seed_into_voice_plan() {
  ResonatorBank bank;
  bank.init(ResonatorBank::Mode::kSim);

  const Seed seed = makeSeed(42, 5.0f, 6.0f, 0.8f, 0.9f, 0.6f, 1, 2);
  const uint32_t when = 2400;

  bank.trigger(seed, when);
  const auto voice = bank.voice(0);

  TEST_ASSERT_TRUE_MESSAGE(voice.active, "Voice should be active after trigger");
  TEST_ASSERT_EQUAL_UINT32_MESSAGE(when, voice.startSample, "Start sample must mirror trigger time");
  TEST_ASSERT_EQUAL_UINT8(42, voice.seedId);
  TEST_ASSERT_EQUAL_UINT8(1, voice.mode);
  TEST_ASSERT_EQUAL_UINT8(2, voice.bank);
  TEST_ASSERT_NOT_NULL_MESSAGE(voice.preset, "Preset pointer should expose the resolved modal bank name");
  TEST_ASSERT_EQUAL_STRING("Kalimba tine", voice.preset);

  const float expectedFreq = 110.0f * std::pow(2.0f, seed.pitch / 12.0f);
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, expectedFreq, voice.frequency);
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 6.0f, voice.burstMs);

  const float expectedDamping = 0.25f + (0.9f - 0.25f) * 0.8f;
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, expectedDamping, voice.damping);

  const float presetBrightness = 0.45f;
  const float expectedBrightness = std::min(1.0f, std::max(0.0f, presetBrightness + (seed.resonator.brightness - presetBrightness) * 0.7f));
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, expectedBrightness, voice.brightness);

  const float presetFeedback = 0.68f;
  const float expectedFeedback = std::min(1.0f, std::max(0.0f, presetFeedback + (seed.resonator.feedback - presetFeedback) * 0.65f));
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, expectedFeedback, voice.feedback);

  const float freqFloor = std::max(10.0f, expectedFreq);
  const float expectedDelaySamples = std::max(1.0f, Units::kSampleRate / freqFloor);
  TEST_ASSERT_FLOAT_WITHIN(1e-3f, expectedDelaySamples, voice.delaySamples);

  const float dampingComp = 1.0f - (expectedDamping - 0.25f) / (0.9f - 0.25f);
  const float gainTilt = 0.45f + (1.25f - 0.45f) * expectedBrightness;
  const float dampingScale = 0.5f + (1.0f - 0.5f) * dampingComp;
  const float expectedBurstGain = gainTilt * dampingScale;
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, expectedBurstGain, voice.burstGain);

  const std::array<float, 4> ratios{1.0f, 2.0f, 3.0f, 4.2f};
  const std::array<float, 4> baseGains{1.0f, 0.5f, 0.35f, 0.2f};
  for (size_t i = 0; i < ratios.size(); ++i) {
    const float expectedModeFreq = expectedFreq * ratios[i];
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, expectedModeFreq, voice.modalFrequencies[i]);

    const float emphasis = (0.6f + (1.4f - 0.6f) * expectedBrightness) * (1.0f - 0.1f * static_cast<float>(i));
    const float expectedModeGain = std::min(1.0f, baseGains[i] * emphasis);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, expectedModeGain, voice.modalGains[i]);
  }
}

void test_resonator_voice_stealing_by_start_then_handle() {
  ResonatorBank bank;
  bank.init(ResonatorBank::Mode::kSim);
  bank.setMaxVoices(3);

  const uint32_t when = 800;
  std::array<ResonatorBank::VoiceState, 3> before{};
  for (uint8_t i = 0; i < 3; ++i) {
    const Seed seed = makeSeed(i + 1, static_cast<float>(i), 4.0f + i, 0.2f + 0.1f * i, 0.3f + 0.1f * i, 0.5f, i % 2, i);
    bank.trigger(seed, when);
    before[i] = bank.voice(i);
  }

  uint8_t expectedSteal = 0;
  uint32_t earliestStart = before[0].startSample;
  uint32_t smallestHandle = before[0].handle;
  for (uint8_t i = 1; i < 3; ++i) {
    if (before[i].startSample < earliestStart ||
        (before[i].startSample == earliestStart && before[i].handle < smallestHandle)) {
      earliestStart = before[i].startSample;
      smallestHandle = before[i].handle;
      expectedSteal = i;
    }
  }

  const Seed overflow = makeSeed(99, 7.5f, 3.0f, 0.4f, 0.9f, 0.85f, 1, 4);
  bank.trigger(overflow, when);

  TEST_ASSERT_EQUAL_UINT8(3, bank.activeVoices());

  std::array<ResonatorBank::VoiceState, 3> after{};
  for (uint8_t i = 0; i < 3; ++i) {
    after[i] = bank.voice(i);
  }

  uint8_t foundOverflow = 3;
  uint32_t maxHandleBefore = 0;
  for (const auto &voice : before) {
    if (voice.handle > maxHandleBefore) {
      maxHandleBefore = voice.handle;
    }
  }

  for (uint8_t i = 0; i < 3; ++i) {
    if (after[i].seedId == 99) {
      foundOverflow = i;
      TEST_ASSERT_TRUE(after[i].handle > maxHandleBefore);
      TEST_ASSERT_EQUAL_UINT32(when, after[i].startSample);
    }
  }

  TEST_ASSERT_NOT_EQUAL(3, foundOverflow);
  TEST_ASSERT_EQUAL_UINT8(expectedSteal, foundOverflow);

  for (uint8_t i = 0; i < 3; ++i) {
    if (i == foundOverflow) {
      continue;
    }
    TEST_ASSERT_EQUAL_UINT32(before[i].handle, after[i].handle);
  }
}

void test_resonator_preset_lookup_guards_index() {
  ResonatorBank bank;
  bank.init(ResonatorBank::Mode::kSim);

  TEST_ASSERT_EQUAL_STRING("Brass shell", bank.presetName(0));
  TEST_ASSERT_EQUAL_STRING(bank.presetName(5), bank.presetName(200));
}

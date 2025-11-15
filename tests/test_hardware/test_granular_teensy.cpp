#include <cstdint>
#include <type_traits>
#include <utility>
#include <unity.h>

#include "Seed.h"
#include "engine/Granular.h"

#if SEEDBOX_HW
#include <Audio.h>

namespace {

// Replicate the compile-time probes from the hardware mapper so we can assert
// against the actual Teensy effect that ships with the core.
template <typename T, typename = void>
struct HasSetGrainLength : std::false_type {};

template <typename T>
struct HasSetGrainLength<T, std::void_t<decltype(std::declval<T &>().setGrainLength(1))>> : std::true_type {};

template <typename T, typename = void>
struct HasBeginPitchShift : std::false_type {};

template <typename T>
struct HasBeginPitchShift<T, std::void_t<decltype(std::declval<T &>().beginPitchShift(1.0f))>> : std::true_type {};

void ensureAudioMemory(uint16_t blocks) {
  static bool allocated = false;
  if (!allocated) {
    AudioMemory(blocks);
    allocated = true;
  }
}

Seed makeSeed(uint8_t id, float pitch, float transpose, float spread, GranularEngine::Source source, uint8_t sdSlot, float grainMs) {
  Seed seed{};
  seed.id = id;
  seed.pitch = pitch;
  seed.prng = 0x600D0000u + static_cast<uint32_t>(id);
  seed.granular.transpose = transpose;
  seed.granular.stereoSpread = spread;
  seed.granular.source = static_cast<uint8_t>(source);
  seed.granular.sdSlot = sdSlot;
  seed.granular.grainSizeMs = grainMs;
  seed.granular.sprayMs = 0.0f;
  seed.granular.windowSkew = -0.2f + 0.05f * id;
  return seed;
}

}  // namespace

void test_teensy_granular_effect_traits() {
  TEST_ASSERT_FALSE(HasSetGrainLength<AudioEffectGranular>::value);
  TEST_ASSERT_TRUE(HasBeginPitchShift<AudioEffectGranular>::value);
}

void test_teensy_granular_assigns_dsp_handles() {
  ensureAudioMemory(GranularEngine::kVoicePoolSize * 2);

  GranularEngine engine;
  engine.init(GranularEngine::Mode::kHardware);

  for (uint8_t i = 0; i < GranularEngine::kVoicePoolSize; ++i) {
    const auto voice = engine.voice(i);
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(i, voice.dspHandle, "Each hardware voice should expose a stable DSP handle");
    TEST_ASSERT_FALSE(voice.active);
  }
}

void test_teensy_granular_triggers_span_mixer_fanout() {
  ensureAudioMemory(GranularEngine::kVoicePoolSize * 2);

  GranularEngine engine;
  engine.init(GranularEngine::Mode::kHardware);
  engine.setMaxActiveVoices(GranularEngine::kVoicePoolSize);
  engine.registerSdClip(1, "kick.raw");
  engine.registerSdClip(2, "snare.raw");
  engine.registerSdClip(3, "hat.raw");

  const uint32_t baseWhen = 512u;
  const uint8_t plannedVoices = 12;  // Enough to step past the first mixer group.
  for (uint8_t i = 0; i < plannedVoices; ++i) {
    const auto source = (i % 3 == 0) ? GranularEngine::Source::kLiveInput : GranularEngine::Source::kSdClip;
    const uint8_t slot = (source == GranularEngine::Source::kSdClip) ? static_cast<uint8_t>((i % 3) + 1) : 0u;
    auto seed = makeSeed(static_cast<uint8_t>(i + 1), -2.0f + 0.5f * i, 0.25f * i, 0.1f * i, source, slot, 65.0f + 2.0f * i);
    engine.trigger(seed, baseWhen + static_cast<uint32_t>(i) * 128u);

    const auto voice = engine.voice(i);
    TEST_ASSERT_TRUE(voice.active);
    TEST_ASSERT_EQUAL_UINT16(i, voice.dspHandle);
    TEST_ASSERT_EQUAL_UINT8(seed.id, voice.seedId);
  }

  TEST_ASSERT_EQUAL_UINT8(plannedVoices, engine.activeVoiceCount());
}

#else  // !SEEDBOX_HW

void test_teensy_granular_effect_traits() {
  TEST_IGNORE_MESSAGE("Teensy-only granular probes run on hardware builds");
}

void test_teensy_granular_assigns_dsp_handles() {
  TEST_IGNORE_MESSAGE("Hardware mixer fan-out checks run on Teensy builds");
}

void test_teensy_granular_triggers_span_mixer_fanout() {
  TEST_IGNORE_MESSAGE("Hardware mixer fan-out checks run on Teensy builds");
}

#endif  // SEEDBOX_HW


#include <array>
#include <unity.h>

#include "Seed.h"
#include "engine/Granular.h"

namespace {
Seed makeSeed(uint32_t id, float pitch, float transpose, float spread, GranularEngine::Source source, uint8_t sdSlot) {
  Seed seed{};
  seed.id = id;
  seed.pitch = pitch;
  seed.prng = 0xBEEF0000u + id;
  seed.granular.transpose = transpose;
  seed.granular.stereoSpread = spread;
  seed.granular.source = static_cast<uint8_t>(source);
  seed.granular.sdSlot = sdSlot;
  seed.granular.grainSizeMs = 75.0f + static_cast<float>(id);
  seed.granular.sprayMs = 0.0f;
  seed.granular.windowSkew = -0.25f + 0.1f * id;
  return seed;
}
}

void test_granular_voice_cap_and_steal() {
  GranularEngine engine;
  engine.init(GranularEngine::Mode::kSim);
  engine.setMaxActiveVoices(3);
  engine.registerSdClip(1, "kick.raw");
  engine.registerSdClip(2, "snare.raw");
  engine.registerSdClip(3, "hat.raw");

  std::array<GranularEngine::GrainVoice, 3> before{};
  const uint32_t baseWhen = 1024u;
  for (uint8_t i = 0; i < before.size(); ++i) {
    auto seed = makeSeed(i + 1, 2.0f * i, 0.5f * i, 0.1f * i, GranularEngine::Source::kSdClip, static_cast<uint8_t>(i + 1));
    engine.trigger(seed, baseWhen + static_cast<uint32_t>(i) * 240u);
    before[i] = engine.voice(i);
    TEST_ASSERT_TRUE(before[i].active);
  }

  uint8_t expectedSteal = 0;
  uint32_t oldestStart = before[0].startSample;
  for (uint8_t i = 1; i < before.size(); ++i) {
    if (before[i].startSample < oldestStart) {
      oldestStart = before[i].startSample;
      expectedSteal = i;
    }
  }

  const Seed overflow = makeSeed(99, -3.0f, 1.5f, 0.75f, GranularEngine::Source::kSdClip, 2);
  const uint32_t newWhen = baseWhen + 720u;
  engine.trigger(overflow, newWhen);

  TEST_ASSERT_EQUAL_UINT8(3, engine.activeVoiceCount());

  uint8_t foundIndex = before.size();
  for (uint8_t i = 0; i < before.size(); ++i) {
    const auto state = engine.voice(i);
    if (state.seedId == overflow.id) {
      foundIndex = i;
      TEST_ASSERT_EQUAL_UINT32(newWhen, state.startSample);
      TEST_ASSERT_EQUAL_UINT16(2, state.sourceHandle);
    } else {
      TEST_ASSERT_EQUAL_UINT8(before[i].seedId, state.seedId);
      TEST_ASSERT_EQUAL_UINT32(before[i].startSample, state.startSample);
    }
  }

  TEST_ASSERT_NOT_EQUAL(before.size(), foundIndex);
  TEST_ASSERT_EQUAL_UINT8(expectedSteal, foundIndex);
}

void test_granular_clamps_zero_cap() {
  GranularEngine engine;
  engine.init(GranularEngine::Mode::kSim);
  engine.setMaxActiveVoices(0);

  const Seed seed = makeSeed(1, 0.0f, 0.0f, 0.2f, GranularEngine::Source::kLiveInput, 0);
  engine.trigger(seed, 2048u);
  TEST_ASSERT_EQUAL_UINT8(1, engine.activeVoiceCount());
  const auto state = engine.voice(0);
  TEST_ASSERT_EQUAL_UINT16(0, state.sourceHandle);
  TEST_ASSERT_NOT_NULL(state.sourcePath);
}

void test_granular_stops_sd_player_when_slot_missing() {
#ifdef SEEDBOX_HW
  TEST_IGNORE_MESSAGE("Simulated hardware helpers only exist in the native build");
#else
  GranularEngine engine;
  engine.init(GranularEngine::Mode::kSim);

  const uint32_t when = 1024u;
  const Seed seed = makeSeed(42, 0.0f, 0.0f, 0.1f, GranularEngine::Source::kSdClip, 7);
  engine.trigger(seed, when);

  const auto simState = engine.simHardwareVoice(0);
  TEST_ASSERT_TRUE_MESSAGE(simState.sdPlayerStopCalled, "Hardware sim should always stop the SD player before mapping a grain");
  TEST_ASSERT_FALSE(simState.sdPlayerPlaying);
  TEST_ASSERT_FALSE(simState.sdPlayerPlayCalled);
  TEST_ASSERT_NULL(simState.lastPlayPath);
#endif
}

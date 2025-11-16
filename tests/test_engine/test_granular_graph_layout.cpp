#include <unity.h>

#include "Seed.h"
#include "engine/Granular.h"

namespace {
Seed makeSeed(uint32_t id, GranularEngine::Source source, uint8_t sdSlot, float spread) {
  Seed seed{};
  seed.id = id;
  seed.pitch = 0.0f;
  seed.prng = 0xAA000000u + id;
  seed.granular.transpose = 0.0f;
  seed.granular.stereoSpread = spread;
  seed.granular.source = static_cast<uint8_t>(source);
  seed.granular.sdSlot = sdSlot;
  seed.granular.grainSizeMs = 60.0f + static_cast<float>(id);
  seed.granular.sprayMs = 0.0f;
  seed.granular.windowSkew = -0.1f;
  return seed;
}
}

void test_granular_graph_layout_tracks_dsp_handles() {
#if SEEDBOX_HW
  TEST_IGNORE_MESSAGE("Graph layout assertions rely on the native simulator");
#else
  GranularEngine engine;
  engine.init(GranularEngine::Mode::kSim);
  constexpr uint8_t kFanIn = GranularEngine::kMixerFanIn;
  constexpr uint8_t kTargetVoices = static_cast<uint8_t>(kFanIn * 2);
  engine.setMaxActiveVoices(kTargetVoices);
  engine.registerSdClip(1, "alpha.raw");
  engine.registerSdClip(2, "beta.raw");

  const uint32_t baseWhen = 8192u;
  for (uint8_t i = 0; i < kTargetVoices; ++i) {
    const bool useSd = (i % 2) == 1;
    const uint8_t slot = useSd ? static_cast<uint8_t>(1 + ((i / 2) % 2)) : 0;
    auto seed = makeSeed(static_cast<uint32_t>(i + 1),
                         useSd ? GranularEngine::Source::kSdClip : GranularEngine::Source::kLiveInput,
                         slot,
                         0.2f + 0.05f * i);
    engine.trigger(seed, baseWhen + static_cast<uint32_t>(i) * 96u);
  }

  for (uint8_t i = 0; i < kTargetVoices; ++i) {
    const auto voice = engine.voice(i);
    TEST_ASSERT_TRUE_MESSAGE(voice.active, "Expected seeded voice to be active");
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(i, voice.dspHandle, "dspHandle should reflect the mixer slot index");

    const bool expectSd = (i % 2) == 1;
    const auto sim = engine.simHardwareVoice(i);
    TEST_ASSERT_TRUE_MESSAGE(sim.sdPlayerStopCalled, "Each trigger should stop the prior SD stream");
    if (i >= kFanIn) {
      TEST_ASSERT_TRUE_MESSAGE(sim.sdPlayerStopCalled,
                               "Voices beyond the first mixer group should still traverse the host lattice");
    }

    if (expectSd) {
      const uint8_t slot = static_cast<uint8_t>(1 + ((i / 2) % 2));
      TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(GranularEngine::Source::kSdClip), static_cast<uint8_t>(voice.source));
      TEST_ASSERT_EQUAL_UINT16(slot, voice.sourceHandle);
      TEST_ASSERT_TRUE(sim.sdPlayerPlayCalled);
      TEST_ASSERT_TRUE(sim.sdPlayerPlaying);
      TEST_ASSERT_NOT_NULL(sim.lastPlayPath);
    } else {
      TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(GranularEngine::Source::kLiveInput), static_cast<uint8_t>(voice.source));
      TEST_ASSERT_EQUAL_UINT16(0, voice.sourceHandle);
      TEST_ASSERT_FALSE(sim.sdPlayerPlayCalled);
      TEST_ASSERT_FALSE(sim.sdPlayerPlaying);
      TEST_ASSERT_NULL(sim.lastPlayPath);
    }
  }

  const uint32_t retriggerWhen = baseWhen + 8192u;
  for (uint8_t i = 0; i < kFanIn; ++i) {
    auto reseed = makeSeed(100u + i, GranularEngine::Source::kSdClip, 1, 0.5f);
    engine.trigger(reseed, retriggerWhen + static_cast<uint32_t>(i) * 128u);
  }

  for (uint8_t i = 0; i < kTargetVoices; ++i) {
    TEST_ASSERT_EQUAL_UINT16(i, engine.voice(i).dspHandle);
  }
#endif
}

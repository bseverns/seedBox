#include <algorithm>
#include <array>
#include <numeric>
#include <unity.h>

#include "Seed.h"
#include "engine/Granular.h"

namespace {
Seed makePerfSeed(uint32_t id, float sizeMs, float sprayMs, uint8_t slot) {
  Seed seed{};
  seed.id = id;
  seed.prng = 0xCAFE0000u + id;
  seed.granular.source = static_cast<uint8_t>(GranularEngine::Source::kSdClip);
  seed.granular.sdSlot = slot;
  seed.granular.grainSizeMs = sizeMs;
  seed.granular.sprayMs = sprayMs;
  seed.granular.windowSkew = 0.0f;
  seed.granular.stereoSpread = 0.5f;
  return seed;
}
}

void test_granular_perf_stats_refresh() {
  GranularEngine engine;
  engine.init(GranularEngine::Mode::kSim);
  engine.armLiveInput(false);
  engine.setMaxActiveVoices(GranularEngine::kVoicePoolSize);

  for (uint8_t slot = 1; slot < GranularEngine::kSdClipSlots; ++slot) {
    engine.registerSdClip(slot, "clip.raw");
  }

  for (uint8_t i = 0; i < GranularEngine::kVoicePoolSize; ++i) {
    const float size = 5.0f + static_cast<float>(i) * 6.0f;
    const float spray = (i % 3 == 0) ? 0.25f * static_cast<float>(i) : 2.5f * static_cast<float>(i % 5);
    const uint8_t slot = static_cast<uint8_t>(1 + (i % std::max<uint8_t>(1, GranularEngine::kSdClipSlots - 1)));
    engine.trigger(makePerfSeed(i + 1, size, spray, slot), 128u * i);
  }

  const auto stats = engine.stats();
  TEST_ASSERT_EQUAL_UINT8(engine.activeVoiceCount(), stats.activeVoiceCount);
  TEST_ASSERT_EQUAL_UINT32(GranularEngine::kVoicePoolSize, stats.grainsPlanned);
  TEST_ASSERT_EQUAL_UINT8(stats.activeVoiceCount, stats.sdOnlyVoiceCount);

  const uint16_t sizeTotal = std::accumulate(stats.grainSizeHistogram.begin(), stats.grainSizeHistogram.end(), uint16_t{0});
  const uint16_t sprayTotal = std::accumulate(stats.sprayHistogram.begin(), stats.sprayHistogram.end(), uint16_t{0});
  TEST_ASSERT_EQUAL_UINT16(stats.activeVoiceCount, sizeTotal);
  TEST_ASSERT_EQUAL_UINT16(stats.activeVoiceCount, sprayTotal);

  bool sawLargeSizeBin = stats.grainSizeHistogram.back() > 0;
  bool sawTinySprayBin = stats.sprayHistogram.front() > 0;
  TEST_ASSERT_TRUE(sawLargeSizeBin);
  TEST_ASSERT_TRUE(sawTinySprayBin);
}

void test_granular_perf_stats_sd_only_runs_drop_back_when_voice_reused() {
  GranularEngine engine;
  engine.init(GranularEngine::Mode::kSim);
  engine.setMaxActiveVoices(1);
  engine.registerSdClip(1, "clip.raw");

  auto sdSeed = makePerfSeed(1, 180.0f, 45.0f, 1);
  engine.trigger(sdSeed, 64);

  auto stats = engine.stats();
  TEST_ASSERT_EQUAL_UINT8(1, stats.activeVoiceCount);
  TEST_ASSERT_EQUAL_UINT8(1, stats.sdOnlyVoiceCount);
  TEST_ASSERT_EQUAL_UINT32(1u, stats.grainsPlanned);

  auto liveSeed = sdSeed;
  liveSeed.id = 2;
  liveSeed.granular.source = static_cast<uint8_t>(GranularEngine::Source::kLiveInput);
  liveSeed.granular.grainSizeMs = 12.0f;
  liveSeed.granular.sprayMs = 0.5f;
  engine.trigger(liveSeed, 96);

  stats = engine.stats();
  TEST_ASSERT_EQUAL_UINT8(1, stats.activeVoiceCount);
  TEST_ASSERT_EQUAL_UINT8(0, stats.sdOnlyVoiceCount);
  TEST_ASSERT_EQUAL_UINT32(2u, stats.grainsPlanned);

  const uint16_t sizeTotal =
      std::accumulate(stats.grainSizeHistogram.begin(), stats.grainSizeHistogram.end(), uint16_t{0});
  const uint16_t sprayTotal =
      std::accumulate(stats.sprayHistogram.begin(), stats.sprayHistogram.end(), uint16_t{0});
  TEST_ASSERT_EQUAL_UINT16(stats.activeVoiceCount, sizeTotal);
  TEST_ASSERT_EQUAL_UINT16(stats.activeVoiceCount, sprayTotal);
}

void test_granular_perf_stats_profiles_mixer_fanout() {
  GranularEngine engine;
  engine.init(GranularEngine::Mode::kSim);
  engine.setMaxActiveVoices(static_cast<uint8_t>(GranularEngine::kMixerFanIn * 3));

  for (uint8_t slot = 1; slot < 4; ++slot) {
    engine.registerSdClip(slot, "clip.raw");
  }

  const uint8_t plannedVoices = static_cast<uint8_t>(GranularEngine::kMixerFanIn * 3);
  for (uint8_t i = 0; i < plannedVoices; ++i) {
    const float size = 20.0f + 3.0f * static_cast<float>(i);
    const float spray = 1.5f * static_cast<float>(i % 5);
    const uint8_t slot = static_cast<uint8_t>(1 + (i % 3));
    engine.trigger(makePerfSeed(i + 1, size, spray, slot), 32u * i);
  }

  const auto stats = engine.stats();
  TEST_ASSERT_EQUAL_UINT8(plannedVoices, stats.activeVoiceCount);
  TEST_ASSERT_EQUAL_UINT8(GranularEngine::kMixerFanIn, stats.busiestMixerLoad);
  TEST_ASSERT_TRUE(stats.mixerGroupsEngaged >= 3);

  const uint16_t mixerTotal =
      std::accumulate(stats.mixerGroupLoad.begin(), stats.mixerGroupLoad.end(), uint16_t{0});
  TEST_ASSERT_EQUAL_UINT16(plannedVoices, mixerTotal);
}

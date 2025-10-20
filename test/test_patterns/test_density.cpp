#include <unity.h>
#include <cmath>
#include <vector>
#include "engine/Patterns.h"
#include "Seed.h"
#include "util/Units.h"

namespace {
struct Counter {
  int calls{0};
};

void triggerCounter(void* ctx, const Seed&, uint32_t) {
  if (!ctx) return;
  reinterpret_cast<Counter*>(ctx)->calls++;
}

struct Hit {
  uint32_t id{0};
  uint32_t when{0};
};

void captureHit(void* ctx, const Seed& seed, uint32_t whenSamples) {
  if (!ctx) return;
  auto* hits = reinterpret_cast<std::vector<Hit>*>(ctx);
  hits->push_back(Hit{seed.id, whenSamples});
}
}

void test_density_gate_runs() {
  PatternScheduler ps; ps.setBpm(120.f);
  Counter counter;
  ps.setTriggerCallback(&counter, triggerCounter);
  Seed s{}; s.density = 2.0f; s.probability = 1.0f; s.jitterMs = 0.f;
  ps.addSeed(s);
  // Just simulate a few ticks to ensure no crashes
  for (int i=0;i<128;++i) ps.onTick();
  TEST_ASSERT_TRUE(ps.ticks() == 128);
  TEST_ASSERT_TRUE(counter.calls > 0);
}

void test_density_fractional_counts() {
  PatternScheduler ps;
  Counter counter;
  ps.setTriggerCallback(&counter, triggerCounter);

  Seed s{};
  s.density = 1.5f;
  s.probability = 1.0f;
  s.jitterMs = 0.f;
  ps.addSeed(s);

  const int ticksToSimulate = 24 * 16; // 16 beats
  for (int i = 0; i < ticksToSimulate; ++i) {
    ps.onTick();
  }

  const int expectedHits = static_cast<int>(16 * s.density);
  TEST_ASSERT_EQUAL_INT(expectedHits, counter.calls);
}

namespace {
std::vector<Hit> runSchedulerForBpm(float bpm, int beats) {
  PatternScheduler ps;
  ps.setBpm(bpm);
  std::vector<Hit> hits;
  ps.setTriggerCallback(&hits, captureHit);

  Seed guaranteed{};
  guaranteed.id = 1u;
  guaranteed.density = 1.0f;
  guaranteed.probability = 1.0f;
  guaranteed.jitterMs = 0.0f;
  ps.addSeed(guaranteed);

  Seed muted{};
  muted.id = 2u;
  muted.density = 1.0f;
  muted.probability = 0.0f;
  muted.jitterMs = 0.0f;
  ps.addSeed(muted);

  const int ticksToSimulate = beats * 24;
  for (int i = 0; i < ticksToSimulate; ++i) {
    ps.onTick();
  }

  return hits;
}

std::vector<uint32_t> hitsForSeed(const std::vector<Hit>& hits, uint32_t id) {
  std::vector<uint32_t> filtered;
  for (const Hit& hit : hits) {
    if (hit.id == id) {
      filtered.push_back(hit.when);
    }
  }
  return filtered;
}

uint32_t expectedDeltaForBpm(float bpm) {
  if (bpm <= 0.0f) {
    return 0u;
  }
  const double samplesPerBeat = (60.0 / static_cast<double>(bpm)) * static_cast<double>(Units::kSampleRate);
  return static_cast<uint32_t>(std::llround(samplesPerBeat));
}
}  // namespace

void test_bpm_drives_sample_cursor() {
  constexpr int kBeatsToSimulate = 6;
  const std::vector<Hit> hits120 = runSchedulerForBpm(120.0f, kBeatsToSimulate);
  const std::vector<Hit> hits60 = runSchedulerForBpm(60.0f, kBeatsToSimulate);

  const std::vector<uint32_t> onBeat120 = hitsForSeed(hits120, 1u);
  const std::vector<uint32_t> onBeat60 = hitsForSeed(hits60, 1u);

  TEST_ASSERT_EQUAL_UINT32(kBeatsToSimulate, static_cast<uint32_t>(onBeat120.size()));
  TEST_ASSERT_EQUAL_UINT32(kBeatsToSimulate, static_cast<uint32_t>(onBeat60.size()));

  TEST_ASSERT_TRUE(onBeat120.size() > 1u);
  TEST_ASSERT_TRUE(onBeat60.size() > 1u);

  const uint32_t delta120 = onBeat120[1] - onBeat120[0];
  const uint32_t delta60 = onBeat60[1] - onBeat60[0];

  TEST_ASSERT_EQUAL_UINT32(expectedDeltaForBpm(120.0f), delta120);
  TEST_ASSERT_EQUAL_UINT32(expectedDeltaForBpm(60.0f), delta60);
  TEST_ASSERT_NOT_EQUAL(delta120, delta60);

  const auto silentHits120 = hitsForSeed(hits120, 2u);
  const auto silentHits60 = hitsForSeed(hits60, 2u);
  TEST_ASSERT_EQUAL_UINT32(0u, static_cast<uint32_t>(silentHits120.size()));
  TEST_ASSERT_EQUAL_UINT32(0u, static_cast<uint32_t>(silentHits60.size()));
}

void test_fractional_bpm_carries_fractional_samples() {
  constexpr float kBpm = 127.5f;
  constexpr int kBeatsToSimulate = 12;
  const std::vector<Hit> hits = runSchedulerForBpm(kBpm, kBeatsToSimulate);

  const std::vector<uint32_t> onBeat = hitsForSeed(hits, 1u);
  TEST_ASSERT_EQUAL_UINT32(kBeatsToSimulate,
                           static_cast<uint32_t>(onBeat.size()));

  TEST_ASSERT_TRUE(onBeat.size() > 2u);

  const double expectedDelta =
      (60.0 / static_cast<double>(kBpm)) * static_cast<double>(Units::kSampleRate);

  bool sawVariation = false;
  uint32_t previousDelta = 0u;
  for (std::size_t i = 1; i < onBeat.size(); ++i) {
    const uint32_t delta = onBeat[i] - onBeat[i - 1];
    const double diff = std::fabs(static_cast<double>(delta) - expectedDelta);
    TEST_ASSERT_TRUE(diff <= 1.0);
    if (i == 1) {
      previousDelta = delta;
    } else if (delta != previousDelta) {
      sawVariation = true;
    }
  }

  TEST_ASSERT_TRUE_MESSAGE(sawVariation,
                           "Fractional BPM should distribute rounding jitter");
}

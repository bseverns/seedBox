#include <unity.h>
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

struct TriggerCapture {
  int calls{0};
  uint32_t lastStart{0};
};

void captureStartSample(void* ctx, const Seed&, uint32_t startSample) {
  if (!ctx) return;
  auto* capture = reinterpret_cast<TriggerCapture*>(ctx);
  capture->calls++;
  capture->lastStart = startSample;
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

void test_scheduler_counts_silent_ticks() {
  Units::simResetSamples();

  PatternScheduler ps;
  TriggerCapture capture;
  ps.setTriggerCallback(&capture, captureStartSample);

  Seed s{};
  s.density = 1.f;
  s.probability = 1.f;
  s.jitterMs = 0.f;
  ps.addSeed(s);

  const int ticksPerBeat = 24;
  for (int i = 0; i < ticksPerBeat - 1; ++i) {
    ps.onTick();
  }
  TEST_ASSERT_EQUAL_INT(0, capture.calls);

  ps.onTick();
  TEST_ASSERT_EQUAL_INT(1, capture.calls);

  const uint32_t expectedSamples = static_cast<uint32_t>(ticksPerBeat * Units::kSimTickSamples);
  TEST_ASSERT_EQUAL_UINT32(expectedSamples, capture.lastStart);
}

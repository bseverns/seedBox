#include <unity.h>
#include "engine/Patterns.h"
#include "Seed.h"

namespace {
struct Counter {
  int calls{0};
};

void triggerCounter(void* ctx, const Seed&, uint32_t) {
  if (!ctx) return;
  reinterpret_cast<Counter*>(ctx)->calls++;
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

// Entry point is shared via test_main.cpp to keep linkers chill.

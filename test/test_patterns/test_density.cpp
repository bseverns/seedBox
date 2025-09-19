#include <unity.h>
#include "engine/Patterns.h"
#include "Seed.h"

void test_density_gate_runs() {
  PatternScheduler ps; ps.setBpm(120.f);
  Seed s{}; s.density = 2.0f; s.probability = 1.0f; s.jitterMs = 0.f;
  ps.addSeed(s);
  // Just simulate a few ticks to ensure no crashes
  for (int i=0;i<128;++i) ps.onTick();
  TEST_ASSERT_TRUE(ps.ticks() == 128);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_density_gate_runs);
  return UNITY_END();
}

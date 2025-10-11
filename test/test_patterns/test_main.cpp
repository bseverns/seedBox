// Linker-friendly entry point for the pattern tests. PlatformIO compiles every
// file in this folder into one binary, so we keep a single main() here and let
// the other translation units just declare test cases.
#include <unity.h>

extern void test_density_gate_runs();
extern void test_density_fractional_counts();
extern void test_plan_grain_sprays_and_mutates_prng();

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_density_gate_runs);
  RUN_TEST(test_density_fractional_counts);
  RUN_TEST(test_plan_grain_sprays_and_mutates_prng);
  return UNITY_END();
}

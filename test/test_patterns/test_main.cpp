#include <unity.h>

void test_density_gate_runs();
void test_density_fractional_counts();
void test_plan_grain_sprays_and_mutates_prng();
void test_map_grain_honors_stereo_spread_width_curve();

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_density_gate_runs);
  RUN_TEST(test_density_fractional_counts);
  RUN_TEST(test_plan_grain_sprays_and_mutates_prng);
  RUN_TEST(test_map_grain_honors_stereo_spread_width_curve);
  return UNITY_END();
}

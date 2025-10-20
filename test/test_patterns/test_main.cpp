#include <unity.h>

void test_density_gate_runs();
void test_density_fractional_counts();
void test_plan_grain_sprays_and_mutates_prng();
void test_bpm_drives_sample_cursor();
void test_fractional_bpm_carries_fractional_samples();

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_density_gate_runs);
  RUN_TEST(test_density_fractional_counts);
  RUN_TEST(test_bpm_drives_sample_cursor);
  RUN_TEST(test_fractional_bpm_carries_fractional_samples);
  RUN_TEST(test_plan_grain_sprays_and_mutates_prng);
  return UNITY_END();
}

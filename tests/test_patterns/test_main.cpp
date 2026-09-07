#include <unity.h>

void test_density_gate_runs();
void test_density_fractional_counts();
void test_scheduler_counts_silent_ticks();
void test_scheduler_bpm_modulates_when_samples();
void test_clock_tick_log_tracks_swing();
void test_plan_grain_sprays_and_mutates_prng();
void test_map_grain_honors_stereo_spread_width_curve();
void test_clock_tick_log_golden();
void test_phenotype_is_bounded_and_preserves_seed_identity();
void test_phenotype_correlates_energy_space_and_stability();
void test_phenotype_mapping_is_deterministic();
void test_siblings_are_deterministic_and_nearer_than_unrelated_phenotypes();
void test_mutation_record_keeps_parent_and_moved_dimensions();

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_density_gate_runs);
  RUN_TEST(test_density_fractional_counts);
  RUN_TEST(test_scheduler_counts_silent_ticks);
  RUN_TEST(test_scheduler_bpm_modulates_when_samples);
  RUN_TEST(test_clock_tick_log_tracks_swing);
  RUN_TEST(test_plan_grain_sprays_and_mutates_prng);
  RUN_TEST(test_map_grain_honors_stereo_spread_width_curve);
  RUN_TEST(test_clock_tick_log_golden);
  RUN_TEST(test_phenotype_is_bounded_and_preserves_seed_identity);
  RUN_TEST(test_phenotype_correlates_energy_space_and_stability);
  RUN_TEST(test_phenotype_mapping_is_deterministic);
  RUN_TEST(test_siblings_are_deterministic_and_nearer_than_unrelated_phenotypes);
  RUN_TEST(test_mutation_record_keeps_parent_and_moved_dimensions);
  return UNITY_END();
}

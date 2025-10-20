#include <unity.h>

void test_granular_voice_cap_and_steal();
void test_granular_clamps_zero_cap();
void test_granular_stops_sd_player_when_slot_missing();
void test_resonator_maps_seed_into_voice_plan();
void test_resonator_voice_stealing_by_start_then_handle();
void test_resonator_preset_lookup_guards_index();
void test_sampler_stores_seed_state();
void test_sampler_voice_stealing_is_oldest_first();

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_granular_voice_cap_and_steal);
  RUN_TEST(test_granular_clamps_zero_cap);
  RUN_TEST(test_granular_stops_sd_player_when_slot_missing);
  RUN_TEST(test_resonator_maps_seed_into_voice_plan);
  RUN_TEST(test_resonator_voice_stealing_by_start_then_handle);
  RUN_TEST(test_resonator_preset_lookup_guards_index);
  RUN_TEST(test_sampler_stores_seed_state);
  RUN_TEST(test_sampler_voice_stealing_is_oldest_first);
  return UNITY_END();
}

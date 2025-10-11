// Engine-suite orchestrator. Unity wants exactly one main() per binary, so we
// rally every engine-focused test case here and let PlatformIO do the rest.
#include <unity.h>

extern void test_granular_voice_cap_and_steal();
extern void test_granular_clamps_zero_cap();
extern void test_resonator_maps_seed_into_voice_plan();
extern void test_resonator_voice_stealing_by_start_then_handle();
extern void test_resonator_preset_lookup_guards_index();
extern void test_sampler_stores_seed_state();
extern void test_sampler_voice_stealing_is_oldest_first();

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_granular_voice_cap_and_steal);
  RUN_TEST(test_granular_clamps_zero_cap);
  RUN_TEST(test_resonator_maps_seed_into_voice_plan);
  RUN_TEST(test_resonator_voice_stealing_by_start_then_handle);
  RUN_TEST(test_resonator_preset_lookup_guards_index);
  RUN_TEST(test_sampler_stores_seed_state);
  RUN_TEST(test_sampler_voice_stealing_is_oldest_first);
  return UNITY_END();
}

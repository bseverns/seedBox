#include <unity.h>

void test_teensy_granular_effect_traits();
void test_teensy_granular_assigns_dsp_handles();
void test_teensy_granular_triggers_span_mixer_fanout();

int main(int, char **) {
  UNITY_BEGIN();
#if SEEDBOX_HW
  RUN_TEST(test_teensy_granular_effect_traits);
  RUN_TEST(test_teensy_granular_assigns_dsp_handles);
  RUN_TEST(test_teensy_granular_triggers_span_mixer_fanout);
#endif
  return UNITY_END();
}


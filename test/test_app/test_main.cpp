#include <unity.h>

void test_external_clock_priority();
void test_cc_cycles_engine_and_snapshot_updates();
void test_engine_selection_persists_and_updates_scheduler();
void test_mn42_handshake_and_clock_follow();
void test_mn42_debug_and_transport_latch_modes();

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_external_clock_priority);
  RUN_TEST(test_cc_cycles_engine_and_snapshot_updates);
  RUN_TEST(test_engine_selection_persists_and_updates_scheduler);
  RUN_TEST(test_mn42_handshake_and_clock_follow);
  RUN_TEST(test_mn42_debug_and_transport_latch_modes);
  return UNITY_END();
}

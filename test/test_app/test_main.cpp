#include <unity.h>

void test_external_clock_priority();
void test_cc_cycles_engine_and_snapshot_updates();
void test_engine_selection_persists_and_updates_scheduler();
void test_mn42_follow_clock_mode();
void test_mn42_debug_meter_toggle();
void test_mn42_transport_latch_behavior();
void test_mn42_usb_channel_normalization();
void test_mn42_hello_resends_ack();
void test_simulator_audio_reports_48k();

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_external_clock_priority);
  RUN_TEST(test_cc_cycles_engine_and_snapshot_updates);
  RUN_TEST(test_engine_selection_persists_and_updates_scheduler);
  RUN_TEST(test_mn42_follow_clock_mode);
  RUN_TEST(test_mn42_debug_meter_toggle);
  RUN_TEST(test_mn42_transport_latch_behavior);
  RUN_TEST(test_mn42_usb_channel_normalization);
  RUN_TEST(test_mn42_hello_resends_ack);
  RUN_TEST(test_simulator_audio_reports_48k);
  return UNITY_END();
}

// Single entry point for the AppState tests so PlatformIO links cleanly. The
// individual test files just describe behaviour; this runner pulls them
// together into one Unity session.
#include <unity.h>

extern void test_external_clock_priority();
extern void test_cc_cycles_engine_and_snapshot_updates();
extern void test_engine_selection_persists_and_updates_scheduler();

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_external_clock_priority);
  RUN_TEST(test_cc_cycles_engine_and_snapshot_updates);
  RUN_TEST(test_engine_selection_persists_and_updates_scheduler);
  return UNITY_END();
}

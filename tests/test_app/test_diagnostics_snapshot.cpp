#include <unity.h>

#include "app/AppState.h"

void test_diagnostics_snapshot_includes_shared_host_counters() {
  AppState app;

  AppState::DiagnosticsSnapshot::HostRuntime host{};
  host.midiDroppedCount = 7;
  host.oversizeBlockDropCount = 3;
  host.lastOversizeBlockFrames = 1024;
  host.preparedScratchFrames = 8192;
  app.setHostDiagnosticsFromHost(host);

  const auto snapshot = app.diagnosticsSnapshot();
  TEST_ASSERT_EQUAL_UINT32(7u, snapshot.host.midiDroppedCount);
  TEST_ASSERT_EQUAL_UINT32(3u, snapshot.host.oversizeBlockDropCount);
  TEST_ASSERT_EQUAL_UINT32(1024u, snapshot.host.lastOversizeBlockFrames);
  TEST_ASSERT_EQUAL_UINT32(8192u, snapshot.host.preparedScratchFrames);
}

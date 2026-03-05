#include <unity.h>

#include <string>

#include "SeedBoxConfig.h"
#include "app/AppState.h"
#include "hal/Board.h"

namespace {
bool contains(const std::string& text, const std::string& needle) {
  return text.find(needle) != std::string::npos;
}
}  // namespace

void test_status_snapshot_reports_focus_seed_and_clock_state() {
  hal::nativeBoardReset();
  AppState app(hal::nativeBoard());
  app.initSim();

  app.setFocusSeed(1);
  app.setSeedEngine(1, EngineRouter::kEuclidId);
  app.setModeFromHost(AppState::Mode::SETTINGS);
  app.setPage(AppState::Page::kStorage);
  app.setClockSourceExternalFromHost(true);
  app.seedPageToggleLock(1);

  AppState::StatusSnapshot status{};
  app.captureStatusSnapshot(status);

  TEST_ASSERT_EQUAL_STRING("SET", status.mode);
  TEST_ASSERT_EQUAL_STRING("Storage", status.page);
  TEST_ASSERT_TRUE(status.externalClockDominant);
  TEST_ASSERT_TRUE(status.followExternalClockEnabled);
  TEST_ASSERT_FALSE(status.globalSeedLocked);
  TEST_ASSERT_TRUE(status.focusSeedLocked);
  TEST_ASSERT_TRUE(status.hasFocusedSeed);
  TEST_ASSERT_EQUAL_UINT8(1, status.focusSeedIndex);
  TEST_ASSERT_EQUAL_UINT32(app.seeds()[1].id, status.focusSeedId);
  TEST_ASSERT_EQUAL_UINT8(EngineRouter::kEuclidId, status.focusSeedEngineId);
  TEST_ASSERT_EQUAL_STRING("Euclid", status.focusSeedEngineName);
  TEST_ASSERT_EQUAL_UINT8(SeedBoxConfig::kQuietMode ? 1u : 0u, status.quietMode ? 1u : 0u);
}

void test_status_json_contains_expected_fields() {
  hal::nativeBoardReset();
  AppState app(hal::nativeBoard());
  app.initSim();

  app.setFocusSeed(1);
  app.setSeedEngine(1, EngineRouter::kEuclidId);
  app.setModeFromHost(AppState::Mode::SETTINGS);
  app.setPage(AppState::Page::kStorage);
  app.setClockSourceExternalFromHost(true);

  const std::string json = app.captureStatusJson();
  TEST_ASSERT_FALSE(json.empty());
  TEST_ASSERT_EQUAL_CHAR('{', json.front());
  TEST_ASSERT_EQUAL_CHAR('}', json.back());
  TEST_ASSERT_TRUE(contains(json, "\"mode\":\"SET\""));
  TEST_ASSERT_TRUE(contains(json, "\"page\":\"Storage\""));
  TEST_ASSERT_TRUE(contains(json, "\"externalClockDominant\":true"));
  TEST_ASSERT_TRUE(contains(json, "\"focusSeed\":{\"present\":true"));
  TEST_ASSERT_TRUE(contains(json, "\"engineId\":3"));
  TEST_ASSERT_TRUE(contains(json, "\"engineName\":\"Euclid\""));

  const std::string seedId = "\"id\":" + std::to_string(app.seeds()[1].id);
  TEST_ASSERT_TRUE(contains(json, seedId));
}

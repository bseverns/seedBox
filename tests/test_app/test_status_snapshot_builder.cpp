#include <unity.h>

#include <string>
#include <vector>

#include "Seed.h"
#include "SeedLock.h"
#include "app/StatusSnapshotBuilder.h"

namespace {
bool contains(const std::string& text, const std::string& needle) {
  return text.find(needle) != std::string::npos;
}
}  // namespace

void test_status_snapshot_builder_defaults_empty_state() {
  StatusSnapshotBuilder builder;
  StatusSnapshotBuilder::Input input{};
  input.mode = "SET";
  input.page = "Storage";
  input.masterSeed = 42;
  input.activePresetId = 7;
  input.bpm = 123.0f;
  input.schedulerTick = 99;
  input.quietMode = true;
  input.focusSeed = 3;

  seedbox::StatusSnapshot status{};
  builder.build(status, input);

  TEST_ASSERT_EQUAL_STRING("SET", status.mode);
  TEST_ASSERT_EQUAL_STRING("Storage", status.page);
  TEST_ASSERT_EQUAL_UINT32(42, status.masterSeed);
  TEST_ASSERT_EQUAL_UINT32(7, status.activePresetId);
  TEST_ASSERT_EQUAL_STRING("default", status.activePresetSlot);
  TEST_ASSERT_TRUE(status.quietMode);
  TEST_ASSERT_FALSE(status.hasFocusedSeed);
  TEST_ASSERT_EQUAL_UINT8(3, status.focusSeedIndex);
  TEST_ASSERT_EQUAL_UINT8(EngineRouter::kSamplerId, status.focusSeedEngineId);
  TEST_ASSERT_EQUAL_STRING("None", status.focusSeedEngineName);
}

void test_status_snapshot_builder_serializes_json_escapes() {
  std::vector<Seed> seeds(1);
  seeds[0].id = 1234;
  seeds[0].engine = EngineRouter::kEuclidId;

  SeedLock locks;
  locks.resize(seeds.size());
  locks.setSeedLocked(0, true);

  StatusSnapshotBuilder builder;
  StatusSnapshotBuilder::Input input{};
  input.mode = "S\"ET";
  input.page = "Clock\\Page";
  input.activePresetSlot = "slot\n1";
  input.focusSeed = 0;
  input.seeds = &seeds;
  input.seedLock = &locks;

  seedbox::StatusSnapshot status{};
  builder.build(status, input);
  const std::string json = builder.toJson(status);

  TEST_ASSERT_TRUE(contains(json, "\"mode\":\"S\\\"ET\""));
  TEST_ASSERT_TRUE(contains(json, "\"page\":\"Clock\\\\Page\""));
  TEST_ASSERT_TRUE(contains(json, "\"activePresetSlot\":\"slot\\n1\""));
  TEST_ASSERT_TRUE(contains(json, "\"focusSeedLocked\":true"));
  TEST_ASSERT_TRUE(contains(json, "\"engineId\":3"));
  TEST_ASSERT_TRUE(contains(json, "\"engineName\":\"Euclid\""));
}

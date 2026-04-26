#include <unity.h>

#include <cstdint>
#include <string>
#include <vector>

#include "app/Preset.h"
#include "app/PresetController.h"

void test_preset_controller_snapshots_and_defaults() {
  PresetController controller;
  controller.setActivePresetSlot("alpha");
  TEST_ASSERT_EQUAL_STRING("alpha", controller.activePresetSlot().c_str());

  std::vector<Seed> seeds(2);
  seeds[0].engine = 1;
  seeds[1].engine = 2;

  std::vector<std::uint8_t> engineSelections{3};
  PresetController::SnapshotInput input{};
  input.masterSeed = 0x12345678u;
  input.focusSeed = 2;
  input.bpm = 126.5f;
  input.followExternal = true;
  input.debugMeters = true;
  input.transportLatch = true;
  input.page = seedbox::PageId::kClock;
  input.seeds = &seeds;
  input.engineSelections = &engineSelections;

  const auto preset = controller.snapshotPreset(input);
  TEST_ASSERT_EQUAL_STRING("default", preset.slot.c_str());
  TEST_ASSERT_EQUAL_UINT32(0x12345678u, preset.masterSeed);
  TEST_ASSERT_EQUAL_UINT8(2, preset.focusSeed);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 126.5f, preset.clock.bpm);
  TEST_ASSERT_TRUE(preset.clock.followExternal);
  TEST_ASSERT_TRUE(preset.clock.debugMeters);
  TEST_ASSERT_TRUE(preset.clock.transportLatch);
  TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(seedbox::PageId::kClock),
                          static_cast<std::uint8_t>(preset.page));
  TEST_ASSERT_EQUAL_size_t(2, preset.engineSelections.size());
  TEST_ASSERT_EQUAL_UINT8(1, preset.engineSelections[0]);
  TEST_ASSERT_EQUAL_UINT8(2, preset.engineSelections[1]);
}

void test_preset_controller_queues_and_crossfades() {
  PresetController controller;
  seedbox::Preset preset{};
  preset.slot = "beta";
  preset.masterSeed = 0xCAFEBABEu;
  preset.focusSeed = 1;
  preset.page = seedbox::PageId::kStorage;

  controller.requestPresetChange(preset, true, PresetController::Boundary::kBar, 95u);
  TEST_ASSERT_FALSE(controller.takePendingPreset(95u).has_value());

  const auto pending = controller.takePendingPreset(96u);
  TEST_ASSERT_TRUE(pending.has_value());
  TEST_ASSERT_EQUAL_STRING("beta", pending->preset.slot.c_str());
  TEST_ASSERT_TRUE(pending->crossfade);
  TEST_ASSERT_EQUAL_UINT64(96u, pending->targetTick);

  TEST_ASSERT_EQUAL_UINT64(96u, controller.computeNextPresetTickForBoundary(PresetController::Boundary::kBar, 95u));
  TEST_ASSERT_EQUAL_UINT64(192u, controller.computeNextPresetTickForBoundary(PresetController::Boundary::kBar, 96u));

  std::vector<Seed> fromSeeds(2);
  fromSeeds[0].id = 10;
  fromSeeds[1].id = 11;
  std::vector<Seed> toSeeds(2);
  toSeeds[0].id = 20;
  toSeeds[1].id = 21;

  controller.beginCrossfade(fromSeeds, toSeeds, 4u);
  TEST_ASSERT_TRUE(controller.crossfadeActive());
  TEST_ASSERT_EQUAL_UINT32(4u, controller.crossfade().remaining);
  TEST_ASSERT_EQUAL_UINT32(4u, controller.crossfade().total);

  controller.decrementCrossfade();
  TEST_ASSERT_EQUAL_UINT32(3u, controller.crossfade().remaining);

  controller.clearCrossfade();
  TEST_ASSERT_FALSE(controller.crossfadeActive());
}

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <unity.h>

#include "app/AppState.h"
#include "interop/mn42_map.h"
#include "interop/mn42_param_map.h"

namespace {
constexpr float kTolerance = 1e-3f;

float normalizedToPitch(uint8_t value) {
  const float normalized = static_cast<float>(value) / 127.f;
  return -24.f + (normalized * 48.f);
}

float normalizedToDensity(uint8_t value) {
  const float normalized = static_cast<float>(value) / 127.f;
  return std::clamp(normalized * 8.f, 0.f, 8.f);
}

float normalizedUnit(uint8_t value) {
  const float normalized = static_cast<float>(value) / 127.f;
  return std::clamp(normalized, 0.f, 1.f);
}

float normalizedToJitter(uint8_t value) {
  const float normalized = static_cast<float>(value) / 127.f;
  return std::clamp(normalized * 30.f, 0.f, 30.f);
}

uint8_t expectedZone(uint8_t value, std::size_t seedCount) {
  if (seedCount == 0) {
    return 0;
  }
  const std::uint32_t scaled = static_cast<std::uint32_t>(value) * static_cast<std::uint32_t>(seedCount);
  const std::size_t zone = std::min<std::size_t>(scaled / 128u, seedCount - 1);
  return static_cast<uint8_t>(zone);
}
}  // namespace

void test_mn42_param_focus_seed_zones() {
  AppState app;
  app.initSim();

  const std::size_t seedCount = app.seeds().size();
  TEST_ASSERT_NOT_EQUAL(0, seedCount);

  app.onExternalControlChange(seedbox::interop::mn42::kDefaultChannel,
                              seedbox::interop::mn42::param::kFocusSeed, 0);
  TEST_ASSERT_EQUAL_UINT8(expectedZone(0, seedCount), app.focusSeed());

  constexpr uint8_t mid = 64;
  app.onExternalControlChange(seedbox::interop::mn42::kDefaultChannel,
                              seedbox::interop::mn42::param::kFocusSeed, mid);
  TEST_ASSERT_EQUAL_UINT8(expectedZone(mid, seedCount), app.focusSeed());

  app.onExternalControlChange(seedbox::interop::mn42::kDefaultChannel,
                              seedbox::interop::mn42::param::kFocusSeed, 127);
  TEST_ASSERT_EQUAL_UINT8(expectedZone(127, seedCount), app.focusSeed());
}

void test_mn42_param_macros_update_seed_fields() {
  AppState app;
  app.initSim();

  app.onExternalControlChange(seedbox::interop::mn42::kDefaultChannel,
                              seedbox::interop::mn42::param::kFocusSeed, 0);
  const uint8_t focus = app.focusSeed();

  app.onExternalControlChange(seedbox::interop::mn42::kDefaultChannel,
                              seedbox::interop::mn42::param::kSeedPitch, 127);
  TEST_ASSERT_FLOAT_WITHIN(kTolerance, normalizedToPitch(127), app.seeds()[focus].pitch);

  constexpr uint8_t densityVal = 80;
  app.onExternalControlChange(seedbox::interop::mn42::kDefaultChannel,
                              seedbox::interop::mn42::param::kSeedDensity, densityVal);
  TEST_ASSERT_FLOAT_WITHIN(kTolerance, normalizedToDensity(densityVal), app.seeds()[focus].density);

  constexpr uint8_t probabilityVal = 45;
  app.onExternalControlChange(seedbox::interop::mn42::kDefaultChannel,
                              seedbox::interop::mn42::param::kSeedProbability, probabilityVal);
  TEST_ASSERT_FLOAT_WITHIN(kTolerance, normalizedUnit(probabilityVal), app.seeds()[focus].probability);

  constexpr uint8_t jitterVal = 100;
  app.onExternalControlChange(seedbox::interop::mn42::kDefaultChannel,
                              seedbox::interop::mn42::param::kSeedJitter, jitterVal);
  TEST_ASSERT_FLOAT_WITHIN(kTolerance, normalizedToJitter(jitterVal), app.seeds()[focus].jitterMs);

  constexpr uint8_t toneVal = 32;
  app.onExternalControlChange(seedbox::interop::mn42::kDefaultChannel,
                              seedbox::interop::mn42::param::kSeedTone, toneVal);
  TEST_ASSERT_FLOAT_WITHIN(kTolerance, normalizedUnit(toneVal), app.seeds()[focus].tone);

  constexpr uint8_t spreadVal = 90;
  app.onExternalControlChange(seedbox::interop::mn42::kDefaultChannel,
                              seedbox::interop::mn42::param::kSeedSpread, spreadVal);
  TEST_ASSERT_FLOAT_WITHIN(kTolerance, normalizedUnit(spreadVal), app.seeds()[focus].spread);

  constexpr uint8_t mutateVal = 58;
  app.onExternalControlChange(seedbox::interop::mn42::kDefaultChannel,
                              seedbox::interop::mn42::param::kSeedMutate, mutateVal);
  TEST_ASSERT_FLOAT_WITHIN(kTolerance, normalizedUnit(mutateVal), app.seeds()[focus].mutateAmt);
}

void test_mn42_param_controls_respect_lock() {
  AppState app;
  app.initSim();

  app.onExternalControlChange(seedbox::interop::mn42::kDefaultChannel,
                              seedbox::interop::mn42::param::kFocusSeed, 0);
  const uint8_t focus = app.focusSeed();
  const float originalTone = app.seeds()[focus].tone;

  app.seedPageToggleLock(focus);
  TEST_ASSERT_TRUE(app.isSeedLocked(focus));

  app.onExternalControlChange(seedbox::interop::mn42::kDefaultChannel,
                              seedbox::interop::mn42::param::kSeedTone, 127);
  TEST_ASSERT_FLOAT_WITHIN(kTolerance, originalTone, app.seeds()[focus].tone);
}

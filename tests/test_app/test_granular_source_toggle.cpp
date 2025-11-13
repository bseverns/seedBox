#include <unity.h>
#include <cstring>
#include <string>
#include "app/AppState.h"
#include "engine/Granular.h"
#include "hal/Board.h"

// This regression test leans heavily on the scripted simulator input helpers.
// The Teensy build does not ship those hooks, so when SEEDBOX_HW is set we
// compile a no-op shim that simply reminds the reader why the case is skipped.
#if !SEEDBOX_HW

namespace {
void runTicks(AppState& app, int count) {
  for (int i = 0; i < count; ++i) {
    app.tick();
  }
}

void enterSeedsPage(AppState& app) {
  hal::nativeBoardFeed("btn seed down");
  hal::nativeBoardFeed("wait 30ms");
  hal::nativeBoardFeed("btn seed up");
  runTicks(app, 24);
}

void performShiftToneTurn(AppState& app, int delta) {
  hal::nativeBoardFeed("btn shift down");
  hal::nativeBoardFeed("wait 30ms");
  const std::string command = std::string("enc tone ") + std::to_string(delta);
  hal::nativeBoardFeed(command.c_str());
  hal::nativeBoardFeed("wait 30ms");
  hal::nativeBoardFeed("btn shift up");
  runTicks(app, 24);
}

uint8_t normalizeSlot(uint8_t slot) {
  if (GranularEngine::kSdClipSlots == 0) {
    return slot;
  }
  return static_cast<uint8_t>(slot % GranularEngine::kSdClipSlots);
}

bool containsToken(const char* text, const char* tokenA, const char* tokenB) {
  if (!text) {
    return false;
  }
  if (tokenA && std::strstr(text, tokenA)) {
    return true;
  }
  if (tokenB && std::strstr(text, tokenB)) {
    return true;
  }
  return false;
}
}  // namespace

void test_granular_source_toggle_via_shift_tone() {
  hal::nativeBoardReset();
  AppState app;
  app.initSim();

  runTicks(app, 2);

  TEST_ASSERT_FALSE(app.seeds().empty());

  enterSeedsPage(app);
  TEST_ASSERT_EQUAL(AppState::Mode::SEEDS, app.mode());

  const uint8_t focus = app.focusSeed();
  app.setSeedEngine(focus, 1);

  const GranularEngine::Source initialSource =
      static_cast<GranularEngine::Source>(app.seeds()[focus].granular.source);
  const uint8_t initialSlot = normalizeSlot(app.seeds()[focus].granular.sdSlot);

  AppState::DisplaySnapshot snap{};

  performShiftToneTurn(app, 1);
  app.captureDisplaySnapshot(snap);

  const Seed& firstSeed = app.seeds()[focus];
  const auto firstSource = static_cast<GranularEngine::Source>(firstSeed.granular.source);
  const uint8_t firstSlot = normalizeSlot(firstSeed.granular.sdSlot);

  const Seed* scheduled = app.debugScheduledSeed(focus);
  TEST_ASSERT_NOT_NULL(scheduled);
  TEST_ASSERT_EQUAL_UINT8(firstSeed.granular.source, scheduled->granular.source);
  TEST_ASSERT_EQUAL_UINT8(firstSeed.granular.sdSlot, scheduled->granular.sdSlot);

  if (initialSource == GranularEngine::Source::kLiveInput) {
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(GranularEngine::Source::kSdClip),
                            static_cast<uint8_t>(firstSource));
    if (GranularEngine::kSdClipSlots > 1) {
      TEST_ASSERT_NOT_EQUAL(initialSlot, firstSlot);
    }
    TEST_ASSERT_TRUE(containsToken(snap.nuance, "gC", "GC"));
  } else {
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(GranularEngine::Source::kLiveInput),
                            static_cast<uint8_t>(firstSource));
    TEST_ASSERT_TRUE(containsToken(snap.nuance, "gL", "GL"));
  }

  performShiftToneTurn(app, 1);
  app.captureDisplaySnapshot(snap);

  const Seed& secondSeed = app.seeds()[focus];
  const auto secondSource = static_cast<GranularEngine::Source>(secondSeed.granular.source);
  const uint8_t secondSlot = normalizeSlot(secondSeed.granular.sdSlot);

  if (initialSource == GranularEngine::Source::kLiveInput) {
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(GranularEngine::Source::kLiveInput),
                            static_cast<uint8_t>(secondSource));
    TEST_ASSERT_TRUE(containsToken(snap.nuance, "gL", "GL"));
  } else {
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(GranularEngine::Source::kSdClip),
                            static_cast<uint8_t>(secondSource));
    if (GranularEngine::kSdClipSlots > 1) {
      TEST_ASSERT_NOT_EQUAL(initialSlot, secondSlot);
    }
    TEST_ASSERT_TRUE(containsToken(snap.nuance, "gC", "GC"));
  }

  performShiftToneTurn(app, 1);
  app.captureDisplaySnapshot(snap);

  const Seed& thirdSeed = app.seeds()[focus];
  const auto thirdSource = static_cast<GranularEngine::Source>(thirdSeed.granular.source);
  const uint8_t thirdSlot = normalizeSlot(thirdSeed.granular.sdSlot);

  if (initialSource == GranularEngine::Source::kLiveInput) {
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(GranularEngine::Source::kSdClip),
                            static_cast<uint8_t>(thirdSource));
    if (GranularEngine::kSdClipSlots > 1) {
      TEST_ASSERT_NOT_EQUAL(firstSlot, thirdSlot);
    }
    TEST_ASSERT_TRUE(containsToken(snap.nuance, "gC", "GC"));
  } else {
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(GranularEngine::Source::kLiveInput),
                            static_cast<uint8_t>(thirdSource));
    TEST_ASSERT_TRUE(containsToken(snap.nuance, "gL", "GL"));
  }
}

#else  // SEEDBOX_HW

void test_granular_source_toggle_via_shift_tone() {
  TEST_IGNORE_MESSAGE(
      "shift+tone source regression only runs against the simulator harness");
}

#endif  // !SEEDBOX_HW

#include <unity.h>
#include <cstring>
#include "app/AppState.h"
#include "engine/Granular.h"

namespace {
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
  AppState app;
  app.initSim();

  TEST_ASSERT_FALSE(app.seeds().empty());

  const uint8_t focus = app.focusSeed();
  app.setSeedEngine(focus, 1);

  const GranularEngine::Source initialSource =
      static_cast<GranularEngine::Source>(app.seeds()[focus].granular.source);
  const uint8_t initialSlot = normalizeSlot(app.seeds()[focus].granular.sdSlot);

  InputEvents::Event evt{};
  evt.type = InputEvents::Type::EncoderHoldTurn;
  evt.encoder = hal::Board::EncoderID::ToneTilt;
  evt.encoderDelta = 1;
  evt.buttons = {hal::Board::ButtonID::Shift};

  AppState::DisplaySnapshot snap{};

  app.handleSeedsEvent(evt);
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

  app.handleSeedsEvent(evt);
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

  app.handleSeedsEvent(evt);
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

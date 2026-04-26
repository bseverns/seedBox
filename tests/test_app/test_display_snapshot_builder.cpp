#include <unity.h>

#include <cstdint>
#include <string>
#include <vector>

#include "SeedLock.h"
#include "app/AppState.h"
#include "app/DisplaySnapshotBuilder.h"

void test_display_snapshot_builder_renders_empty_state() {
  DisplaySnapshotBuilder builder;
  seedbox::DisplaySnapshot snap{};
  UiState ui{};
  SeedLock seedLock{};

  DisplaySnapshotBuilder::Input input{};
  input.masterSeed = 0x12345678u;
  input.sampleRate = 48000.0f;
  input.framesPerBlock = 512;
  input.ledOn = true;
  input.audioCallbackCount = 17;
  input.frame = 29;
  input.mode = static_cast<std::uint8_t>(AppState::Mode::HOME);
  input.currentPage = static_cast<std::uint8_t>(AppState::Page::kSeeds);
  input.seedPrimeMode = static_cast<std::uint8_t>(AppState::SeedPrimeMode::kLfsr);
  input.gateDivision = static_cast<std::uint8_t>(AppState::GateDivision::kBars);
  input.focusSeed = 0;
  input.bpm = 120.0f;
  input.swing = 0.0f;
  input.externalClockDominant = false;
  input.waitingForExternalClock = false;
  input.debugMetersEnabled = false;
  input.seedPrimeBypassEnabled = false;
  input.followExternalClockEnabled = false;
  input.inputGateHot = false;
  input.gateEdgePending = false;
  input.seedLock = &seedLock;

  builder.build(snap, ui, input);

  TEST_ASSERT_EQUAL_STRING("SeedBox 345678", snap.title);
  TEST_ASSERT_EQUAL_STRING("HOME empty", snap.status);
  TEST_ASSERT_EQUAL_STRING("SR48.0kB512", snap.metrics);
  TEST_ASSERT_EQUAL_STRING("AC00017F00029", snap.nuance);
  TEST_ASSERT_EQUAL(UiState::Mode::kPerformance, ui.mode);
  TEST_ASSERT_EQUAL_STRING("Idle", ui.engineName.data());
  TEST_ASSERT_EQUAL_STRING("Gate:Shift+Den", ui.pageHints[0].data());
  TEST_ASSERT_EQUAL_STRING("Tap:LFSR GBAR", ui.pageHints[1].data());
}

void test_display_snapshot_builder_switches_settings_hints() {
  DisplaySnapshotBuilder builder;
  seedbox::DisplaySnapshot snap{};
  UiState ui{};
  SeedLock seedLock{};

  DisplaySnapshotBuilder::Input input{};
  input.masterSeed = 0x0000BEEFu;
  input.mode = static_cast<std::uint8_t>(AppState::Mode::SETTINGS);
  input.currentPage = static_cast<std::uint8_t>(AppState::Page::kStorage);
  input.seedPrimeMode = static_cast<std::uint8_t>(AppState::SeedPrimeMode::kTapTempo);
  input.seedPrimeBypassEnabled = true;
  input.followExternalClockEnabled = true;
  input.seedLock = &seedLock;

  builder.build(snap, ui, input);

  TEST_ASSERT_EQUAL(UiState::Mode::kPerformance, ui.mode);
  TEST_ASSERT_EQUAL_STRING("SET bypass", snap.status);
  TEST_ASSERT_EQUAL_STRING("Alt:prime skip", ui.pageHints[0].data());
  TEST_ASSERT_EQUAL_STRING("Tap:ext clock", ui.pageHints[1].data());
}

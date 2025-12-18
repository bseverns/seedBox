#include <cstdint>
#include <cstring>
#include <string>
#include <unity.h>
#include "app/AppState.h"
#include "app/EngineRouter.h"
#include "ui/AsciiOledView.h"

namespace {

constexpr uint8_t kEngineCycleCc = 20;

std::string RenderFrame(AppState& app) {
  AppState::DisplaySnapshot snap{};
  UiState ui{};
  app.captureDisplaySnapshot(snap, ui);
  ui::AsciiOledView view(false);
  view.present(snap, ui);
  return view.latest();
}

}  // namespace

void test_ascii_frame_matches_boot_snapshot() {
  AppState app;
  app.initSim();

  const std::string frame = RenderFrame(app);
  TEST_ASSERT_FALSE(frame.empty());
  TEST_ASSERT_NOT_NULL(strstr(frame.c_str(), "SeedBox"));
  TEST_ASSERT_NOT_NULL(strstr(frame.c_str(), "PRFI"));
}

void test_ascii_renderer_tracks_engine_swaps() {
  AppState app;
  app.initSim();

  ui::AsciiOledView view(false);
  {
    AppState::DisplaySnapshot snap{};
    UiState ui{};
    app.captureDisplaySnapshot(snap, ui);
    view.present(snap, ui);
  }
  const std::string first = view.latest();

  app.onExternalControlChange(0, kEngineCycleCc, 127);
  AppState::DisplaySnapshot updated{};
  UiState updatedUi{};
  app.captureDisplaySnapshot(updated, updatedUi);
  view.present(updated, updatedUi);

  TEST_ASSERT_TRUE(view.hasFrames());
  TEST_ASSERT_GREATER_OR_EQUAL_UINT(2, static_cast<unsigned>(view.frames().size()));
  TEST_ASSERT_FALSE(std::strcmp(first.c_str(), view.latest().c_str()) == 0);
  TEST_ASSERT_NOT_NULL(strstr(view.latest().c_str(), "GRA"));
}

void test_debug_meter_toggle_updates_metrics_and_mode() {
  AppState app;
  app.initSim();

  const auto focus = app.focusSeed();
  app.setSeedEngine(focus, EngineRouter::kResonatorId);

  AppState::DisplaySnapshot baseline{};
  UiState baselineUi{};
  app.captureDisplaySnapshot(baseline, baselineUi);

  const std::string baselineMetrics(baseline.metrics);

  app.setDebugMetersEnabledFromHost(true);

  AppState::DisplaySnapshot debugSnap{};
  UiState debugUi{};
  app.captureDisplaySnapshot(debugSnap, debugUi);

  TEST_ASSERT_EQUAL(UiState::Mode::kSystem, debugUi.mode);
  TEST_ASSERT_NOT_EQUAL(0, std::strcmp(baseline.metrics, debugSnap.metrics));
  TEST_ASSERT_NOT_NULL(strstr(debugSnap.metrics, "F"));
}


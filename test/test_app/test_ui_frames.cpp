#include <cstdint>
#include <cstring>
#include <string>
#include <unity.h>
#include "app/AppState.h"
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


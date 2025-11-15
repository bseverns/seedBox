#include <cstdio>
#include <cstring>
#include <string>
#include <unity.h>
#include "app/AppState.h"
#include "ui/AsciiOledView.h"

namespace {

std::string CaptureFrame(AppState& app) {
  AppState::DisplaySnapshot snapshot{};
  UiState ui{};
  app.captureDisplaySnapshot(snapshot, ui);
  ui::AsciiOledView view(false);
  view.present(snapshot, ui);
  TEST_ASSERT_TRUE_MESSAGE(view.hasFrames(), "AsciiOledView did not capture a frame");
  return view.latest();
}

std::string FirstLine(const std::string& frame) {
  const std::size_t newline = frame.find('\n');
  if (newline == std::string::npos) {
    return frame;
  }
  return frame.substr(0, newline);
}

void PrintFrame(const char* label, const std::string& frame) {
  std::printf("\n[%s]\n%s\n", label, frame.c_str());
  std::fflush(stdout);
}

char LockGlyph(const std::string& statusLine) {
  if (statusLine.size() < UiState::kTextColumns) {
    return '\0';
  }
  // ComposeTextFrame formats the status banner as 16 chars with the lock flag
  // sitting right before the trailing space.
  return statusLine[UiState::kTextColumns - 2];
}

char ClockGlyph(const std::string& statusLine) {
  return statusLine.size() > 3 ? statusLine[3] : '\0';
}

}  // namespace

void test_ui_gallery_snapshots() {
  {
    AppState app;
    app.initSim();
    app.seedPageToggleGlobalLock();
    const std::string frame = CaptureFrame(app);
    PrintFrame("global-lock", frame);
    const std::string banner = FirstLine(frame);
    TEST_ASSERT_EQUAL_CHAR('L', LockGlyph(banner));
  }

  {
    AppState app;
    app.initSim();
    app.enterSwingMode();
    app.adjustSwing(0.17f);
    const std::string frame = CaptureFrame(app);
    PrintFrame("swing-edit", frame);
    const std::string banner = FirstLine(frame);
    TEST_ASSERT_TRUE(banner.rfind("EDT", 0) == 0);
    TEST_ASSERT_NOT_EQUAL(std::string::npos, frame.find("SW17"));
  }

  {
    AppState app;
    app.initSim();
    app.onExternalTransportStart();
    const std::string frame = CaptureFrame(app);
    PrintFrame("external-clock", frame);
    const std::string banner = FirstLine(frame);
    TEST_ASSERT_EQUAL_CHAR('E', ClockGlyph(banner));
  }
}

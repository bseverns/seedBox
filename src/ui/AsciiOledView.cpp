#include "ui/AsciiOledView.h"
#include <cstdio>
#include <sstream>
#include "ui/TextFrame.h"

namespace ui {

AsciiOledView::AsciiOledView(bool logToStdout) : log_(logToStdout) {}

void AsciiOledView::present(const AppState::DisplaySnapshot& snapshot, const UiState& state) {
  const TextFrame frame = ComposeTextFrame(snapshot, state);
  std::ostringstream oss;
  for (std::size_t i = 0; i < frame.lineCount; ++i) {
    if (i != 0) {
      oss << '\n';
    }
    oss << frame.lines[i].data();
  }
  const std::string rendered = oss.str();
  if (!frames_.empty() && frames_.back() == rendered) {
    return;
  }
  frames_.push_back(rendered);
  if (log_) {
    std::printf("[oled:%zu]\n%s\n", frames_.size(), rendered.c_str());
    std::fflush(stdout);
  }
}

}  // namespace ui


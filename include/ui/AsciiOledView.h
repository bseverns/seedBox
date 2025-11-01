#pragma once

#include <string>
#include <vector>
#include "app/AppState.h"
#include "app/UiState.h"

namespace ui {

class AsciiOledView {
public:
  explicit AsciiOledView(bool logToStdout = true);

  void present(const AppState::DisplaySnapshot& snapshot, const UiState& state);
  const std::vector<std::string>& frames() const { return frames_; }
  bool hasFrames() const { return !frames_.empty(); }
  const std::string& latest() const { return frames_.back(); }

private:
  bool log_{true};
  std::vector<std::string> frames_{};
};

}  // namespace ui


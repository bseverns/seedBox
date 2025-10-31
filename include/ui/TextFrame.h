#pragma once

//
// TextFrame — display-neutral layout helper
// -----------------------------------------
// Both the hardware OLED view and the native ASCII renderer want to present the
// exact same pixels.  `TextFrame` captures that layout as a fixed set of 16-char
// lines so we can diff frames in tests and trust that the SH1107 panel will see
// the same content.
#include <array>
#include "app/AppState.h"
#include "app/UiState.h"

namespace ui {

struct TextFrame {
  static constexpr std::size_t kMaxLines = 8;
  std::array<std::array<char, UiState::kTextColumns + 1>, kMaxLines> lines{};
  std::size_t lineCount{0};
};

TextFrame ComposeTextFrame(const AppState::DisplaySnapshot& snapshot, const UiState& state);
bool operator==(const TextFrame& a, const TextFrame& b);
inline bool operator!=(const TextFrame& a, const TextFrame& b) { return !(a == b); }

}  // namespace ui


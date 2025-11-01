#include "ui/TextFrame.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string_view>
#include "SeedBoxConfig.h"

namespace ui {
namespace {

void copyLine(std::array<char, UiState::kTextColumns + 1>& dst, std::string_view text) {
  const std::size_t maxCopy = UiState::kTextColumns;
  const std::size_t copyLen = std::min(maxCopy, text.size());
  if (copyLen > 0) {
    std::memcpy(dst.data(), text.data(), copyLen);
  }
  dst[copyLen] = '\0';
  if (copyLen < maxCopy) {
    std::fill(dst.begin() + static_cast<std::ptrdiff_t>(copyLen + 1), dst.end(), '\0');
  }
}

const char* modeTag(UiState::Mode mode) {
  switch (mode) {
    case UiState::Mode::kEdit: return "EDT";
    case UiState::Mode::kSystem: return "SYS";
    case UiState::Mode::kPerformance:
    default:
      return "PRF";
  }
}

char clockGlyph(UiState::ClockSource source) {
  return source == UiState::ClockSource::kExternal ? 'E' : 'I';
}

std::array<char, 4> engineTag(const UiState& state) {
  std::array<char, 4> tag{'-', '-', '-', '\0'};
  std::size_t cursor = 0;
  for (char c : state.engineName) {
    if (c == '\0') {
      break;
    }
    if (!std::isalnum(static_cast<unsigned char>(c))) {
      continue;
    }
    tag[cursor++] = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    if (cursor >= 3) {
      break;
    }
  }
  while (cursor < 3) {
    tag[cursor++] = '-';
  }
  tag[3] = '\0';
  return tag;
}

int clampBpm(float bpm) {
  const float clamped = std::clamp(bpm, 0.0f, 999.0f);
  return static_cast<int>(std::lround(clamped));
}

int clampSwing(float swing) {
  const float raw = (swing > 1.0f) ? swing : swing * 100.0f;
  const float clamped = std::clamp(raw, 0.0f, 99.0f);
  return static_cast<int>(std::lround(clamped));
}

std::array<char, UiState::kTextColumns + 1> composeStatus(const UiState& state) {
  std::array<char, UiState::kTextColumns + 1> line{};
  const char* mode = modeTag(state.mode);
  const char clock = clockGlyph(state.clock);
  const int bpm = clampBpm(state.bpm);
  const int swing = clampSwing(state.swing);
  const auto engine = engineTag(state);
  const char lock = state.seedLocked ? 'L' : '-';
  std::snprintf(line.data(), line.size(), "%s%c%03dSW%02d%s%c ", mode, clock, bpm, swing, engine.data(), lock);
  line[UiState::kTextColumns] = '\0';
  return line;
}

bool isEmpty(const char* text) {
  return !text || text[0] == '\0';
}

}  // namespace

TextFrame ComposeTextFrame(const AppState::DisplaySnapshot& snapshot, const UiState& state) {
  TextFrame frame{};
  auto append = [&](std::string_view text) {
    if (text.empty()) {
      return;
    }
    if (frame.lineCount >= TextFrame::kMaxLines) {
      return;
    }
    copyLine(frame.lines[frame.lineCount++], text);
  };

  const auto statusLine = composeStatus(state);
  append(statusLine.data());

  if constexpr (SeedBoxConfig::kQuietMode) {
    append("QUIET MODE ARMED");
  }

  if (!isEmpty(snapshot.title)) {
    append(snapshot.title);
  }
  if (!isEmpty(snapshot.status)) {
    append(snapshot.status);
  }
  if (!isEmpty(snapshot.metrics)) {
    append(snapshot.metrics);
  }
  if (!isEmpty(snapshot.nuance)) {
    append(snapshot.nuance);
  }

  for (const auto& hint : state.pageHints) {
    if (hint[0] == '\0') {
      continue;
    }
    append(hint.data());
  }

  return frame;
}

bool operator==(const TextFrame& a, const TextFrame& b) {
  if (a.lineCount != b.lineCount) {
    return false;
  }
  for (std::size_t i = 0; i < a.lineCount; ++i) {
    if (std::strncmp(a.lines[i].data(), b.lines[i].data(), UiState::kTextColumns) != 0) {
      return false;
    }
  }
  return true;
}

}  // namespace ui


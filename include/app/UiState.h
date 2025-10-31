#pragma once

//
// UiState — front-panel cheat sheet
// ---------------------------------
// A lightweight snapshot of everything the display layer needs to narrate what
// the firmware is doing.  AppState owns the truth and hydrates this struct so
// renderers (hardware OLED or native ASCII) can stay pixel-accurate without
// duplicating business logic.
#include <array>
#include <cstdint>
#include <cstddef>

struct UiState {
  static constexpr std::size_t kTextColumns = 16;
  static constexpr std::size_t kHintCount = 2;

  enum class Mode : std::uint8_t {
    kPerformance,
    kEdit,
    kSystem,
  };

  enum class ClockSource : std::uint8_t {
    kInternal,
    kExternal,
  };

  Mode mode{Mode::kPerformance};
  float bpm{0.0f};
  float swing{0.0f};
  ClockSource clock{ClockSource::kInternal};
  bool seedLocked{false};
  std::array<char, kTextColumns + 1> engineName{};
  std::array<std::array<char, kTextColumns + 1>, kHintCount> pageHints{};
};


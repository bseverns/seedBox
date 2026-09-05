#pragma once

#include <array>
#include "hal/Board.h"

// Current Board control inventory shared by firmware, simulator, and JUCE.
// Labels are panel legends; tokens are the stable native script vocabulary.
namespace hal::panel {
struct Encoder {
  Board::ButtonID button_id;
  std::uint8_t pin_switch;
  std::uint8_t pin_a;
  std::uint8_t pin_b;
  const char* label;
  const char* token;
  Board::EncoderID id;
};
struct Button {
  Board::ButtonID id;
  std::uint8_t pin;
  const char* label;
  const char* token;
};
inline constexpr std::array<Encoder, 4> encoders{{
    {Board::ButtonID::EncoderSeedBank, 2, 0, 1, "Seed/Bank", "seed", Board::EncoderID::SeedBank},
    {Board::ButtonID::EncoderDensity, 5, 3, 4, "Density", "density", Board::EncoderID::Density},
    {Board::ButtonID::EncoderToneTilt, 27, 24, 26, "Tone/Tilt", "tone", Board::EncoderID::ToneTilt},
    {Board::ButtonID::EncoderFxMutate, 30, 6, 9, "FX/Mutate", "fx", Board::EncoderID::FxMutate},
}};
inline constexpr std::array<Button, 4> buttons{{
    {Board::ButtonID::TapTempo, 31, "Tap Tempo", "tap"},
    {Board::ButtonID::Shift, 32, "Shift", "shift"},
    {Board::ButtonID::AltSeed, 33, "Alt Seed", "alt"},
    {Board::ButtonID::LiveCapture, 34, "Live Capture", "capture"},
}};
inline constexpr std::size_t buttonCount = encoders.size() + buttons.size();

constexpr bool sameText(const char* a, const char* b) {
  if (!a || !b) return a == b;
  for (; *a && *a == *b; ++a, ++b) {}
  return *a == *b;
}
constexpr bool validText(const char* text) {
  if (!text || !*text) return false;
  for (; *text; ++text) if (*text == '\t' || *text == '\n' || *text == '\r') return false;
  return true;
}
constexpr bool uniquePins() {
  std::array<std::uint8_t, encoders.size() * 3 + buttons.size()> pins{};
  std::size_t n = 0;
  for (const auto& control : encoders) {
    pins[n++] = control.pin_a;
    pins[n++] = control.pin_b;
    pins[n++] = control.pin_switch;
  }
  for (const auto& control : buttons) pins[n++] = control.pin;
  for (std::size_t i = 0; i < pins.size(); ++i)
    for (std::size_t j = i + 1; j < pins.size(); ++j)
      if (pins[i] == pins[j]) return false;
  return true;
}
constexpr bool completeButtonMapping() {
  std::array<bool, static_cast<std::size_t>(Board::ButtonID::Count)> seen{};
  // Encoder IDs and their integrated switch IDs use the same ordinal order.
  for (std::size_t i = 0; i < encoders.size(); ++i) {
    const auto id = static_cast<std::size_t>(encoders[i].button_id);
    if (id != i || static_cast<std::size_t>(encoders[i].id) != i ||
        id >= seen.size() || seen[id]) return false;
    seen[id] = true;
  }
  for (const auto& control : buttons) {
    const auto id = static_cast<std::size_t>(control.id);
    if (id >= seen.size() || seen[id]) return false;
    seen[id] = true;
  }
  for (bool mapped : seen) if (!mapped) return false;
  return true;
}
constexpr bool uniqueNames() {
  std::array<const char*, buttonCount> labels{}, tokens{};
  std::size_t n = 0;
  for (const auto& control : encoders) { labels[n] = control.label; tokens[n++] = control.token; }
  for (const auto& control : buttons) { labels[n] = control.label; tokens[n++] = control.token; }
  for (std::size_t i = 0; i < n; ++i) {
    if (!validText(labels[i]) || !validText(tokens[i])) return false;
    for (const char* p = tokens[i]; *p; ++p)
      if (!(*p >= 'a' && *p <= 'z')) return false;
    for (std::size_t j = i + 1; j < n; ++j)
      if (sameText(labels[i], labels[j]) || sameText(tokens[i], tokens[j])) return false;
  }
  return true;
}
static_assert(encoders.size() == static_cast<std::size_t>(Board::EncoderID::Count),
              "Panel encoder count does not cover Board::EncoderID");
static_assert(buttonCount == static_cast<std::size_t>(Board::ButtonID::Count),
              "Panel button count does not cover Board::ButtonID");
static_assert(buttonCount <= 32, "InputEvents button mask holds at most 32 switches");
static_assert(uniquePins(), "Panel GPIO pins must be unique");
static_assert(completeButtonMapping(), "Panel switches must map every Board button exactly once in encoder order");
static_assert(uniqueNames(), "Panel labels and lowercase script tokens must be nonempty and unique");
}  // namespace hal::panel

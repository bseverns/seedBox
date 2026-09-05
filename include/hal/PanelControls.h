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
};
struct Button {
  Board::ButtonID id;
  std::uint8_t pin;
  const char* label;
  const char* token;
};
inline constexpr std::array<Encoder, 4> encoders{{
    {Board::ButtonID::EncoderSeedBank, 2, 0, 1, "Seed/Bank", "seed"},
    {Board::ButtonID::EncoderDensity, 5, 3, 4, "Density", "density"},
    {Board::ButtonID::EncoderToneTilt, 27, 24, 26, "Tone/Tilt", "tone"},
    {Board::ButtonID::EncoderFxMutate, 30, 6, 9, "FX/Mutate", "fx"},
}};
inline constexpr std::array<Button, 4> buttons{{
    {Board::ButtonID::TapTempo, 31, "Tap Tempo", "tap"},
    {Board::ButtonID::Shift, 32, "Shift", "shift"},
    {Board::ButtonID::AltSeed, 33, "Alt Seed", "alt"},
    {Board::ButtonID::LiveCapture, 34, "Live Capture", "capture"},
}};
inline constexpr std::size_t buttonCount = encoders.size() + buttons.size();
}  // namespace hal::panel

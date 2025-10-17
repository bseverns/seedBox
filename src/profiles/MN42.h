#pragma once

// Quick lookup for the MN42 controller's CC layout.  Having the numbers named
// makes it way easier to narrate "twist this knob, watch that parameter" in a
// workshop.
#include <stdint.h>
namespace MN42CC {
  constexpr uint8_t SEED_INDEX = 20;
  constexpr uint8_t DENSITY    = 23;
  constexpr uint8_t PROB       = 24;
  constexpr uint8_t JITTER     = 25;
  constexpr uint8_t MUTATE     = 26;
  constexpr uint8_t TONE       = 30;
  constexpr uint8_t SPREAD     = 31;
  constexpr uint8_t ENV_A      = 40;
  constexpr uint8_t ENV_D      = 41;
  constexpr uint8_t ENV_S      = 42;
  constexpr uint8_t ENV_R      = 43;
}

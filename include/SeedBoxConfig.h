#pragma once

//
// Build-time switches.
// --------------------
// A single header that exposes the knobs we expect builders to tweak.  We keep
// the list small so workshops can flip options without spelunking through build
// scripts.
#ifndef QUIET_MODE
#define QUIET_MODE 1
#endif

namespace SeedBoxConfig {
// Quiet mode flips off anything that would touch real hardware (MIDI, SD, audio
// IO).  Great for unit tests and for running the firmware in a classroom where
// you may not have a Teensy attached.
constexpr bool kQuietMode = (QUIET_MODE != 0);
}

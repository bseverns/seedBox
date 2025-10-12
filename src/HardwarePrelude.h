#pragma once

// Pulling in Arduino's core before the Teensy Audio graph headers keeps the
// platform macros (TEENSYDUINO, F_CPU_ACTUAL, etc.) wired up correctly. CI
// caught that omission the hard way, so this header now codifies the include
// order everywhere we spin up the hardware graph.
#ifdef SEEDBOX_HW
#include <Arduino.h>
#include <Audio.h>
#include <memory>
#include <vector>
#endif

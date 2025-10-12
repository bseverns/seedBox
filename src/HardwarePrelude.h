#pragma once

// Pulling in Arduino's core before the Teensy Audio graph headers keeps the
// platform macros (TEENSYDUINO, F_CPU_ACTUAL, etc.) wired up correctly. CI
// caught that omission the hard way, so this header now codifies the include
// order everywhere we spin up the hardware graph.
#ifdef SEEDBOX_HW

#include <Arduino.h>

// PlatformIO's Teensy builds occasionally drop the compiler-provided
// `__ARM_ARCH_7EM__` define when they set up the cross compiler. The Teensy
// Audio library uses that flag to expose the Cortex-M optimized mixer and
// granular effect classes. When the define goes missing those headers only
// forward-declare the classes, leaving them abstract and exploding our build
// with "no member named gain" errors. We patch the macro back in once the
// Arduino core has told us which silicon we're riding so CI stays honest.
#if (defined(__IMXRT1062__) || defined(ARDUINO_TEENSY40) || defined(ARDUINO_TEENSY41)) && !defined(__ARM_ARCH_7EM__)
#define __ARM_ARCH_7EM__ 1
#endif

#include <Audio.h>
#include <memory>
#include <vector>
#endif

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
// with "no member named gain" errors. CI surfaced a case where *none* of the
// Teensy-specific ID macros (`__IMXRT1062__`, `ARDUINO_TEENSY40`, etc.) were
// defined yet we were still compiling the hardware graph. To keep the audio
// nodes concrete we patch the architecture define back in as soon as we know
// we're in a hardware build.
#if !defined(__ARM_ARCH_7EM__)
#define __ARM_ARCH_7EM__ 1
#endif

// Older TeensyDuino cores rely on `__IMXRT1062__` to expose the full feature
// set. PlatformIO occasionally misplaces it, so we mirror the guard here to
// make sure the granular/mixer classes grow real bodies.
#if !defined(__IMXRT1062__) && (defined(ARDUINO_TEENSY40) || defined(ARDUINO_TEENSY41))
#define __IMXRT1062__ 1
#endif

#include <Audio.h>
#include <memory>
#include <vector>
#endif

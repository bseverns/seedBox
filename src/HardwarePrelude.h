#pragma once

// Pulling in Arduino's core before the Teensy Audio graph headers keeps the
// platform macros (TEENSYDUINO, F_CPU_ACTUAL, etc.) wired up correctly. CI
// caught that omission the hard way, so this header now codifies the include
// order everywhere we spin up the hardware graph.
#ifdef SEEDBOX_HW

#ifndef ARDUINO_TEENSY40
#define ARDUINO_TEENSY40 1
#endif

// A bunch of the Teensy Audio headers hinge on TEENSYDUINO >= some-version to
// decide whether to hand out real class definitions or just ghostly forward
// declares. PlatformIO sometimes forgets to set the macro when it strips down
// the board config for unit tests, so we resurrect it here with a suitably new
// version tag.
#ifndef TEENSYDUINO
#define TEENSYDUINO 158
#endif

// PlatformIO's Teensy builds occasionally drop the compiler-provided
// `__ARM_ARCH_7EM__` define when they set up the cross compiler. The Teensy
// Audio library uses that flag to expose the Cortex-M optimized mixer and
// granular effect classes. When the define goes missing those headers only
// forward-declare the classes, leaving them abstract and exploding our build
// with "no member named gain" errors. CI surfaced a case where *none* of the
// Teensy-specific ID macros were defined yet we were still compiling the
// hardware graph. To keep the audio nodes concrete we patch the architecture
// define back in as soon as we know we're in a hardware build.
#if !defined(__ARM_ARCH_7EM__)
#define __ARM_ARCH_7EM__ 1
#endif

// Older TeensyDuino cores rely on `__IMXRT1062__` to expose the full feature
// set. PlatformIO occasionally misplaces it, so we mirror the guard here to
// make sure the granular/mixer classes grow real bodies even if the board ID
// vanishes from the compiler flags.
#if !defined(__IMXRT1062__)
#define __IMXRT1062__ 1
#endif

#include <Arduino.h>

#include <Audio.h>
#include <memory>
#include <vector>
#endif

#pragma once

// This header gets force-included by the Teensy PlatformIO environments so the
// downstream PJRC Audio library always sees the identifiers it needs to hand out
// real class definitions. When those macros go missing the library quietly
// downgrades to forward declarations and the whole graph implodes with
// abstract-class errors.
#ifdef SEEDBOX_HW

#ifndef ARDUINO_TEENSY40
#define ARDUINO_TEENSY40 1
#endif

#ifndef TEENSYDUINO
#define TEENSYDUINO 158
#endif

#ifndef __ARM_ARCH_7EM__
#define __ARM_ARCH_7EM__ 1
#endif

#ifndef __IMXRT1062__
#define __IMXRT1062__ 1
#endif

#endif  // SEEDBOX_HW

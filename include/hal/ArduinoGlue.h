#pragma once

#ifdef SEEDBOX_HW
// Teensy's usb_midi.h uses legacy function-pointer casts that trigger GCC's
// -Wcast-function-type warning. We hush that noise while including Arduino.h so
// CI stays green without muting the rest of the compilation unit.
#  ifdef __GNUC__
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wcast-function-type"
#  endif
#  include <Arduino.h>
#  ifdef __GNUC__
#    pragma GCC diagnostic pop
#  endif
#endif


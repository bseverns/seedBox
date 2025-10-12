#pragma once

#ifdef SEEDBOX_HW
#  include "hal/ArduinoGlue.h"
#  ifdef __GNUC__
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wcast-function-type"
#  endif
#  include <usb_midi.h>
#  ifdef __GNUC__
#    pragma GCC diagnostic pop
#  endif
#endif


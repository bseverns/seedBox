#pragma once
#include <stdint.h>

class MidiRouter {
public:
  void begin();
  void onUsbEvent();
  void onControlChange(uint8_t ch, uint8_t cc, uint8_t val);
  void onClockTick();
  void onStart();
  void onStop();
};

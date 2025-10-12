#include "hal/hal_io.h"

#ifdef SEEDBOX_HW
  #include "hal/ArduinoGlue.h"
#endif

namespace seedbox::hal {

namespace {
IoConfig g_config{};
IoCounters g_counters{};
bool g_led_state = false;
}

void init_io(const IoConfig& config) {
  g_config = config;
  g_counters = IoCounters{};
  g_led_state = false;
#ifdef SEEDBOX_HW
  pinMode(g_config.status_led_pin, OUTPUT);
  set_status_led(false);
#endif
}

void shutdown_io() {
#ifdef SEEDBOX_HW
  set_status_led(false);
#endif
}

void set_status_led(bool on) {
  g_led_state = on;
#ifdef SEEDBOX_HW
  const bool level = g_config.led_active_high ? on : !on;
  digitalWriteFast(g_config.status_led_pin, level ? HIGH : LOW);
#endif
}

bool status_led() { return g_led_state; }

void note_midi_event() {
  ++g_counters.midi_events;
  set_status_led(true);
}

void note_encoder_turn(std::int32_t delta) {
  if (delta != 0) {
    ++g_counters.encoder_turns;
  }
}

IoCounters counters() { return g_counters; }

}  // namespace seedbox::hal

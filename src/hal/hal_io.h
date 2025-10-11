#pragma once

#include <cstdint>

namespace seedbox::hal {

struct IoConfig {
  std::uint8_t status_led_pin = 13;
  bool led_active_high = true;
};

struct IoCounters {
  std::uint32_t midi_events = 0;
  std::uint32_t encoder_turns = 0;
};

void init_io(const IoConfig& config = IoConfig{});
void shutdown_io();

void set_status_led(bool on);
bool status_led();

void note_midi_event();
void note_encoder_turn(std::int32_t delta);
IoCounters counters();

}  // namespace seedbox::hal

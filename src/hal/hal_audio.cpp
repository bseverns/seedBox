#include "hal/hal_audio.h"

#ifdef SEEDBOX_HW
  #include <Arduino.h>
  #include <Audio.h>
#endif

namespace seedbox::hal {

namespace {
AudioSettings g_settings{};
AudioMetrics g_metrics{};

#ifdef SEEDBOX_HW
AudioControlSGTL5000 g_codec;
#endif

}  // namespace

void init_audio(const AudioSettings& settings) {
  g_settings = settings;
  g_metrics = AudioMetrics{settings.sample_rate_hz, settings.frames_per_buffer, 0};
#ifdef SEEDBOX_HW
  AudioMemory(settings.memory_blocks);
  g_codec.enable();
  g_codec.volume(settings.output_volume);
#else
  (void)settings;
#endif
}

void shutdown_audio() {
#ifdef SEEDBOX_HW
  g_codec.disable();
#endif
}

AudioMetrics audio_metrics() { return g_metrics; }

#ifndef SEEDBOX_HW
void mock_dispatch_buffer() {
  ++g_metrics.buffers_dispatched;
}
#endif

}  // namespace seedbox::hal

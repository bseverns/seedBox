#include "harness.h"

#include <cmath>
#include <vector>

#include "hal/hal_audio.h"

namespace seedbox::tests::golden {

RenderReport render_fixture(const RenderOptions& options) {
  RenderReport report;
  report.hash = "TODO_FILL_HASH";
  (void)options;
#ifdef ENABLE_GOLDEN
  seedbox::hal::AudioSettings settings{};
  settings.sample_rate_hz = 48000.0f;
  settings.frames_per_buffer = 128;
  seedbox::hal::init_audio(settings);

  const std::size_t total_samples = static_cast<std::size_t>(
      settings.sample_rate_hz * options.duration_seconds);
  std::vector<std::int16_t> buffer(total_samples, 0);
  report.samples = buffer.size();
  report.wrote_fixture = false;
#else
  (void)options;
  report.samples = 0;
  report.wrote_fixture = false;
#endif
  return report;
}

void write_wav_placeholder(const std::string& path,
                           const std::int16_t* samples,
                           std::size_t sample_count) {
  (void)path;
  (void)samples;
  (void)sample_count;
  // TODO: implement a tiny PCM16 writer once fixtures are allowed.
}

}  // namespace seedbox::tests::golden

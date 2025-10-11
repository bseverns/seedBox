#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace seedbox::tests::golden {

struct RenderOptions {
  std::string name;
  float duration_seconds = 1.0f;
  bool quiet_mode = true;
};

struct RenderReport {
  std::string hash = "TODO_FILL_HASH";
  std::size_t samples = 0;
  bool wrote_fixture = false;
};

// Produce a deterministic render under ENABLE_GOLDEN. When the flag is disabled
// the harness reports a skipped run.
RenderReport render_fixture(const RenderOptions& options);

// Placeholder for a 16-bit PCM writer. Intentionally not wired yet so we do not
// accidentally ship binary fixtures.
void write_wav_placeholder(const std::string& path,
                           const std::int16_t* samples,
                           std::size_t sample_count);

}  // namespace seedbox::tests::golden

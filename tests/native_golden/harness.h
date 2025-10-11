#pragma once

#include <cstddef>
#include <cstdint>

namespace seedbox::tests::golden {

struct RenderSpec {
  std::size_t frames;
  std::uint32_t sampleRate;
};

struct RenderResult {
  bool rendered;
  const char* note;
};

// Render deterministic audio into `outBuffer`. The caller owns the buffer and
// guarantees it has at least `spec.frames` samples per channel.
RenderResult renderSproutFixture(float* outBuffer, std::size_t channels,
                                 const RenderSpec& spec);
RenderResult renderReseedFixture(float* outBuffer, std::size_t channels,
                                 const RenderSpec& spec);

// Placeholder writer for 16-bit PCM WAV output. The implementation currently
// returns false and is a TODO for the next hardening pass.
bool writeWav16(const char* path, const std::int16_t* samples,
                std::size_t frameCount, std::uint32_t sampleRate,
                std::size_t channels);

}  // namespace seedbox::tests::golden

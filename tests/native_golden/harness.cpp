#include "tests/native_golden/harness.h"

namespace seedbox::tests::golden {

namespace {
RenderResult makePending(const char* label) {
  return RenderResult{false, label};
}
}  // namespace

RenderResult renderSproutFixture(float*, std::size_t, const RenderSpec&) {
  return makePending("TODO: render sprout fixture");
}

RenderResult renderReseedFixture(float*, std::size_t, const RenderSpec&) {
  return makePending("TODO: render reseed fixture");
}

bool writeWav16(const char*, const std::int16_t*, std::size_t,
                std::uint32_t, std::size_t) {
  return false;  // TODO: implement 16-bit PCM writer without heap allocations.
}

}  // namespace seedbox::tests::golden

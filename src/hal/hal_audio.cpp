#include "hal/hal_audio.h"

#include "SeedboxConfig.h"

#ifdef SEEDBOX_HW
  #include <Audio.h>
#endif

namespace {
#ifdef SEEDBOX_HW
AudioControlSGTL5000 gCodec;
constexpr std::uint16_t kAudioMemoryBlocks = 64;
#endif
constexpr seedbox::hal::AudioTiming kTiming{48000u, 128u};
}  // namespace

namespace seedbox::hal {

void bootAudioBackend() {
#ifdef SEEDBOX_HW
  AudioMemory(kAudioMemoryBlocks);
  gCodec.enable();
  gCodec.volume(0.6f);
#else
  // Native builds set up their own mock streams during tests.
#endif
}

AudioTiming timing() { return kTiming; }

}  // namespace seedbox::hal

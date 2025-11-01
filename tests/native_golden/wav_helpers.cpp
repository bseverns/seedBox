#include "wav_helpers.hpp"

#include <cstddef>
#include <iomanip>
#include <sstream>

#include "SeedBoxConfig.h"

namespace golden {

bool write_wav_16(const WavWriteRequest &request) {
    (void)request;
#if ENABLE_GOLDEN
    // TODO: Actually write out a WAV header + PCM data once we bless the fixture flow.
    return false;
#else
    // Intentionally a no-op so native tests can run fast with ENABLE_GOLDEN=0.
    return false;
#endif
}

std::string hash_pcm16(const std::vector<int16_t> &samples) {
#if ENABLE_GOLDEN
    // TODO: Replace with a real hash (xxHash, SHA-256, whatever keeps you honest).
    std::ostringstream oss;
    oss << std::hex << samples.size();
    return oss.str();
#else
    // Deterministic but clearly fake hash reminding devs to enable the flag.
    return "TODO_FILL_HASH";
#endif
}

}  // namespace golden

#include <iostream>
#include <unity.h>

#include "SeedBoxConfig.h"
#include "wav_helpers.hpp"

namespace {
void setUp(void) {}
void tearDown(void) {}

#if ENABLE_GOLDEN
void test_emit_flag_matrix() {
    std::cout << "[seedbox-config] active flag matrix" << std::endl;
    for (const auto &flag : SeedBoxConfig::kFlagMatrix) {
        std::cout << "  " << flag.name << "=" << (flag.enabled ? "1" : "0")
                  << " // " << flag.story << std::endl;
    }
    std::cout << std::flush;
    TEST_MESSAGE("Flag matrix dumped for golden log capture.");
}

void test_render_and_compare_golden() {
    golden::WavWriteRequest request{};
    request.path = "build/fixtures/drone-intro.wav";
    request.sample_rate_hz = 48000;
    request.samples.assign(48000, 0);  // TODO: swap in real render buffers.

    const bool write_ok = golden::write_wav_16(request);
    const std::string hash = golden::hash_pcm16(request.samples);

    (void)write_ok;
    (void)hash;

    TEST_IGNORE_MESSAGE(
        "Golden fixtures not yet implemented. Capture audio, hash it, and update golden.json.");
}
#else
void test_golden_mode_disabled() {
    TEST_PASS_MESSAGE(
        "ENABLE_GOLDEN is off. No fixtures rendered; this test only ensures the harness compiles.");
}
#endif
}  // namespace

int main(int, char **) {
    UNITY_BEGIN();
#if ENABLE_GOLDEN
    RUN_TEST(test_emit_flag_matrix);
    RUN_TEST(test_render_and_compare_golden);
#else
    RUN_TEST(test_golden_mode_disabled);
#endif
    return UNITY_END();
}

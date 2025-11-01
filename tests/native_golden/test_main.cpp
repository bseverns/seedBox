#include <algorithm>
#include <cmath>
#include <vector>
#include <unity.h>

#include "wav_helpers.hpp"

#ifndef ENABLE_GOLDEN
#define ENABLE_GOLDEN 0
#endif

namespace {

constexpr std::size_t kDroneFrames = 48000;
constexpr double kSampleRate = 48000.0;
constexpr double kDroneFreqHz = 110.0;
constexpr double kDroneAmplitude = 0.5;
constexpr char kExpectedHash[] = "f53315eb7db89d33";

std::vector<int16_t> make_drone() {
    std::vector<int16_t> samples(kDroneFrames);
    constexpr double kTwoPi = 6.283185307179586476925286766559;
    for (std::size_t i = 0; i < kDroneFrames; ++i) {
        const double t = static_cast<double>(i) / kSampleRate;
        const double cycle = std::sin(kTwoPi * kDroneFreqHz * t);
        const double scaled = std::clamp(cycle * kDroneAmplitude, -1.0, 1.0);
        const auto quantized = static_cast<long>(std::lround(scaled * 32767.0));
        samples[i] = static_cast<int16_t>(std::clamp<long>(quantized, -32768L, 32767L));
    }
    return samples;
}

void setUp(void) {}
void tearDown(void) {}

#if ENABLE_GOLDEN
void test_render_and_compare_golden() {
    golden::WavWriteRequest request{};
    request.path = "build/fixtures/drone-intro.wav";
    request.sample_rate_hz = static_cast<uint32_t>(kSampleRate);
    request.samples = make_drone();

    const bool write_ok = golden::write_wav_16(request);
    const std::string hash = golden::hash_pcm16(request.samples);

    TEST_ASSERT_TRUE_MESSAGE(write_ok, "Failed to write golden WAV fixture");
    TEST_ASSERT_EQUAL_STRING(kExpectedHash, hash.c_str());
}
#else
void test_golden_mode_disabled() {
    golden::WavWriteRequest request{};
    request.sample_rate_hz = static_cast<uint32_t>(kSampleRate);
    request.samples = make_drone();

    const std::string hash = golden::hash_pcm16(request.samples);
    TEST_ASSERT_EQUAL_STRING(kExpectedHash, hash.c_str());
}
#endif
}  // namespace

int main(int, char **) {
    UNITY_BEGIN();
#if ENABLE_GOLDEN
    RUN_TEST(test_render_and_compare_golden);
#else
    RUN_TEST(test_golden_mode_disabled);
#endif
    return UNITY_END();
}

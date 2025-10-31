#include <unity.h>

#include "wav_helpers.hpp"

#include "io/MidiRouter.h"

#include <array>
#include <cstdint>
#include <cstddef>

#ifndef ENABLE_GOLDEN
#define ENABLE_GOLDEN 0
#endif

namespace {
void setUp(void) {}
void tearDown(void) {}

#if !defined(SEEDBOX_HW)
void test_cli_backend_clock_routing() {
    MidiRouter router;
    router.begin();

    std::array<MidiRouter::RouteConfig, MidiRouter::kPortCount> perf{};
    for (auto& cfg : perf) {
        cfg.acceptControlChange = true;
    }
    const std::size_t usbIndex = static_cast<std::size_t>(MidiRouter::Port::kUsb);
    perf[usbIndex].acceptClock = true;
    router.configurePageRouting(MidiRouter::Page::kPerf, perf);

    bool clockSeen = false;
    router.setClockHandler([&]() { clockSeen = true; });

    auto* usbCli = router.cliBackend(MidiRouter::Port::kUsb);
    usbCli->pushClock();
    router.poll();

    TEST_ASSERT_TRUE(clockSeen);
}

void test_cli_channel_map_and_panic() {
    MidiRouter router;
    router.begin();

    std::array<MidiRouter::RouteConfig, MidiRouter::kPortCount> perf{};
    for (auto& cfg : perf) {
        cfg.acceptControlChange = true;
    }
    router.configurePageRouting(MidiRouter::Page::kPerf, perf);

    MidiRouter::ChannelMap usbMap;
    for (auto& entry : usbMap.inbound) {
        entry = static_cast<std::uint8_t>((entry + 1u) % 16u);
    }
    for (auto& entry : usbMap.outbound) {
        entry = static_cast<std::uint8_t>((entry + 2u) % 16u);
    }
    router.setChannelMap(MidiRouter::Port::kUsb, usbMap);

    std::uint8_t observedChannel = 0xFFu;
    router.setControlChangeHandler(
        [&](std::uint8_t channel, std::uint8_t, std::uint8_t) { observedChannel = channel; });

    auto* usbCli = router.cliBackend(MidiRouter::Port::kUsb);
    usbCli->pushControlChange(0u, 10u, 64u);
    router.poll();
    TEST_ASSERT_EQUAL_UINT8(1u, observedChannel);

    usbCli->clearSent();
    router.sendNoteOn(MidiRouter::Port::kUsb, 0u, 60u, 100u);
    router.sendNoteOff(MidiRouter::Port::kUsb, 0u, 60u, 0u);
    router.sendNoteOff(MidiRouter::Port::kUsb, 0u, 60u, 0u);  // Guarded duplicate.
    router.sendNoteOn(MidiRouter::Port::kUsb, 3u, 67u, 120u);
    router.panic();

    const auto& sent = usbCli->sentMessages();
    TEST_ASSERT_EQUAL_size_t(4u, sent.size());
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(MidiRouter::CliBackend::SentMessage::Type::kNoteOn),
                            static_cast<std::uint8_t>(sent[0].type));
    TEST_ASSERT_EQUAL_UINT8(2u, sent[0].channel);
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(MidiRouter::CliBackend::SentMessage::Type::kNoteOff),
                            static_cast<std::uint8_t>(sent[1].type));
    TEST_ASSERT_EQUAL_UINT8(2u, sent[1].channel);
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(MidiRouter::CliBackend::SentMessage::Type::kNoteOn),
                            static_cast<std::uint8_t>(sent[2].type));
    TEST_ASSERT_EQUAL_UINT8(5u, sent[2].channel);
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(MidiRouter::CliBackend::SentMessage::Type::kAllNotesOff),
                            static_cast<std::uint8_t>(sent[3].type));
    TEST_ASSERT_EQUAL_UINT8(5u, sent[3].channel);
}

void test_cli_trs_transport_mirror() {
    MidiRouter router;
    router.begin();

    std::array<MidiRouter::RouteConfig, MidiRouter::kPortCount> perf{};
    const std::size_t usbIndex = static_cast<std::size_t>(MidiRouter::Port::kUsb);
    const std::size_t trsIndex = static_cast<std::size_t>(MidiRouter::Port::kTrsA);
    perf[usbIndex].acceptControlChange = true;
    perf[usbIndex].mirrorTransport = true;
    perf[trsIndex].acceptTransport = true;
    router.configurePageRouting(MidiRouter::Page::kPerf, perf);

    bool startSeen = false;
    router.setStartHandler([&]() { startSeen = true; });

    auto* usbCli = router.cliBackend(MidiRouter::Port::kUsb);
    auto* trsCli = router.cliBackend(MidiRouter::Port::kTrsA);
    usbCli->clearSent();
    trsCli->pushStart();
    router.poll();

    TEST_ASSERT_TRUE(startSeen);
    const auto& sent = usbCli->sentMessages();
    TEST_ASSERT_EQUAL_size_t(1u, sent.size());
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(MidiRouter::CliBackend::SentMessage::Type::kStart),
                            static_cast<std::uint8_t>(sent[0].type));
}
#endif

#if ENABLE_GOLDEN
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
    RUN_TEST(test_render_and_compare_golden);
#else
#if !defined(SEEDBOX_HW)
    RUN_TEST(test_cli_backend_clock_routing);
    RUN_TEST(test_cli_channel_map_and_panic);
    RUN_TEST(test_cli_trs_transport_mirror);
#endif
    RUN_TEST(test_golden_mode_disabled);
#endif
    return UNITY_END();
}

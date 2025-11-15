#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

// Offline helper that mirrors the PlatformIO-native golden renders. We compile
// this with a stock desktop compiler so contributors can regenerate
// `build/fixtures/*` and refresh the manifest even when PlatformIO can't grab
// the native toolchain (think CI outages, captive portals, or lab sessions
// without internet). The routines below are copy/pasted siblings of the helpers
// in tests/native_golden/test_main.cpp; keep them in sync.

#define ENABLE_GOLDEN 1

#include "Seed.h"
#include "SeedBoxConfig.h"
#include "engine/BurstEngine.h"
#include "engine/EuclidEngine.h"
#include "engine/Granular.h"
#include "engine/Resonator.h"
#include "engine/Sampler.h"
#include "tests/native_golden/wav_helpers.hpp"

#include "../examples/shared/offline_renderer.hpp"
#include "../examples/shared/reseed_playbook.hpp"

namespace {

constexpr std::size_t kDroneFrames = 48000;
constexpr double kSampleRate = 48000.0;

struct GoldenFixture {
    const char* name;
    const char* path;
    const char* expected_hash;
};

constexpr GoldenFixture kAudioFixtures[] = {
    {"drone-intro", "build/fixtures/drone-intro.wav", "f53315eb7db89d33"},
    {"sampler-grains", "build/fixtures/sampler-grains.wav", "630fbfadca574688"},
    {"resonator-tail", "build/fixtures/resonator-tail.wav", "e329aa6faffb39f4"},
    {"granular-haze", "build/fixtures/granular-haze.wav", "0ba2bd0c8e981ef5"},
    {"mixer-console", "build/fixtures/mixer-console.wav", "3ba5f705dc237611"},
    {"quad-bus", "build/fixtures/quad-bus.wav", "2e1656a463a978d5"},
    {"surround-bus", "build/fixtures/surround-bus.wav", "dbafae2da8a1ac60"},
    {"long-random-take", "build/fixtures/long-random-take.wav", "97e954c9e2a909df"},
    {"reseed-A", "build/fixtures/reseed-A.wav", "6bdbf0d855da618f"},
    {"reseed-B", "build/fixtures/reseed-B.wav", "998a54390940abbc"},
};

constexpr GoldenFixture kLogFixtures[] = {
    {"euclid-mask", "build/fixtures/euclid-mask.txt", "2431091b3af7d347"},
    {"burst-cluster", "build/fixtures/burst-cluster.txt", "082de9ac9a3cb359"},
    {"reseed-log", "build/fixtures/reseed-log.json", "873935e11a5704f3"},
};

enum class FixtureKind { kAudio, kLog };

std::string to_lower_copy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::vector<std::string> parse_filter_tokens() {
    const char* raw = std::getenv("SEEDBOX_OFFLINE_GOLDEN_FILTER");
    if (raw == nullptr || *raw == '\0') {
        return {};
    }

    std::vector<std::string> tokens;
    std::string current;
    const std::string source(raw);

    auto flush = [&]() {
        if (current.empty()) {
            return;
        }
        tokens.push_back(to_lower_copy(current));
        current.clear();
    };

    for (char ch : source) {
        if (ch == ',' || ch == ';' || std::isspace(static_cast<unsigned char>(ch))) {
            flush();
        } else {
            current.push_back(ch);
        }
    }
    flush();

    tokens.erase(
        std::remove_if(tokens.begin(), tokens.end(), [](const std::string& token) { return token.empty(); }),
        tokens.end());
    return tokens;
}

bool token_matches_kind(const std::string& token, FixtureKind kind) {
    if (token == "all" || token == "*") {
        return true;
    }
    if (kind == FixtureKind::kAudio) {
        return token == "audio" || token == "audios" || token == "wav" || token == "stems";
    }
    return token == "log" || token == "logs" || token == "txt" || token == "text";
}

bool should_emit(const GoldenFixture& spec,
                 FixtureKind kind,
                 const std::vector<std::string>& tokens) {
    if (tokens.empty()) {
        return true;
    }

    const std::string name = to_lower_copy(spec.name);
    const std::string path = to_lower_copy(spec.path);
    for (const auto& token : tokens) {
        if (token.empty()) {
            continue;
        }
        if (token_matches_kind(token, kind)) {
            return true;
        }
        if (name.find(token) != std::string::npos) {
            return true;
        }
        if (path.find(token) != std::string::npos) {
            return true;
        }
    }
    return false;
}

bool emit_control_log(const char* fixture_name, const std::string& body);

std::vector<int16_t> render_reseed_variant(std::uint32_t master_seed,
                                           const char* control_fixture_name = nullptr);

std::vector<int16_t> make_drone() {
    std::vector<int16_t> samples(kDroneFrames);
    constexpr double kTwoPi = 6.283185307179586476925286766559;
    constexpr double kDroneFreqHz = 110.0;
    constexpr double kDroneAmplitude = 0.5;
    for (std::size_t i = 0; i < kDroneFrames; ++i) {
        const double t = static_cast<double>(i) / kSampleRate;
        const double cycle = std::sin(kTwoPi * kDroneFreqHz * t);
        const double scaled = std::clamp(cycle * kDroneAmplitude, -1.0, 1.0);
        const auto quantized = static_cast<long>(std::lround(scaled * 32767.0));
        samples[i] = static_cast<int16_t>(std::clamp<long>(quantized, -32768L, 32767L));
    }
    std::ostringstream control;
    control << "# drone-intro control log" << '\n';
    control << "frames=" << kDroneFrames << " sample_rate_hz=" << static_cast<int>(kSampleRate)
            << " freq_hz=" << kDroneFreqHz << " amplitude=" << kDroneAmplitude << '\n';
    (void)emit_control_log("drone-intro", control.str());
    return samples;
}

struct SamplerVoiceSpec {
    Seed seed;
    std::uint32_t when;
};

Seed make_sampler_seed(std::uint32_t id,
                       std::uint8_t sample_idx,
                       float pitch,
                       float attack,
                       float decay,
                       float sustain,
                       float release,
                       float tone,
                       float spread) {
    Seed seed{};
    seed.id = id;
    seed.sampleIdx = sample_idx;
    seed.pitch = pitch;
    seed.envA = attack;
    seed.envD = decay;
    seed.envS = sustain;
    seed.envR = release;
    seed.tone = tone;
    seed.spread = spread;
    seed.engine = static_cast<std::uint8_t>(Engine::Type::kSampler);
    return seed;
}

double sampler_adsr(double t,
                    double attack,
                    double decay,
                    double sustain,
                    double release,
                    double sustain_hold) {
    if (t < 0.0) {
        return 0.0;
    }

    const double kMinStage = 1e-4;
    attack = std::max(attack, kMinStage);
    decay = std::max(decay, kMinStage);
    release = std::max(release, kMinStage);

    if (t < attack) {
        return t / attack;
    }
    t -= attack;
    if (t < decay) {
        const double progress = t / decay;
        return 1.0 + (sustain - 1.0) * progress;
    }
    t -= decay;
    if (t < sustain_hold) {
        return sustain;
    }
    t -= sustain_hold;
    if (t < release) {
        const double progress = t / release;
        return sustain * (1.0 - progress);
    }
    return 0.0;
}

std::vector<int16_t> render_sampler_fixture() {
    Sampler sampler;
    sampler.init();

    const SamplerVoiceSpec voices[] = {
        {make_sampler_seed(1, 0, -5.0f, 0.01f, 0.08f, 0.6f, 0.2f, 0.35f, 0.1f), 0u},
        {make_sampler_seed(2, 3, 2.0f, 0.005f, 0.12f, 0.5f, 0.35f, 0.65f, 0.45f), 8000u},
        {make_sampler_seed(3, 6, 9.0f, 0.02f, 0.18f, 0.4f, 0.5f, 0.8f, 0.85f), 16000u},
    };

    std::ostringstream control;
    control << "# sampler-grains control log" << '\n';
    control << "frames=" << kDroneFrames << " sample_rate_hz=" << static_cast<int>(kSampleRate)
            << " voices=" << std::size(voices) << '\n';
    control << "voice,when_samples,id,sample,pitch,envA,envD,envS,envR,tone,spread" << '\n';
    control << std::fixed << std::setprecision(4);

    for (std::size_t idx = 0; idx < std::size(voices); ++idx) {
        const auto& spec = voices[idx];
        sampler.trigger(spec.seed, spec.when);
        control << idx << ',' << spec.when << ',' << spec.seed.id << ','
                << static_cast<int>(spec.seed.sampleIdx) << ',' << spec.seed.pitch << ','
                << spec.seed.envA << ',' << spec.seed.envD << ',' << spec.seed.envS << ','
                << spec.seed.envR << ',' << spec.seed.tone << ',' << spec.seed.spread << '\n';
    }
    (void)emit_control_log("sampler-grains", control.str());

    std::vector<double> mix(kDroneFrames, 0.0);
    const double sustain_hold = 0.25;
    const double baseFrequencies[] = {110.0, 164.81, 220.0, 261.63, 329.63, 392.0, 523.25};

    for (std::uint8_t i = 0; i < Sampler::kMaxVoices; ++i) {
        const auto voice = sampler.voice(i);
        if (!voice.active) {
            continue;
        }
        const double freq_base = baseFrequencies[voice.sampleIndex % std::size(baseFrequencies)];
        const double freq = freq_base * static_cast<double>(voice.playbackRate);
        const double pan_gain = 0.5 * (static_cast<double>(voice.leftGain) + static_cast<double>(voice.rightGain));
        for (std::size_t frame = 0; frame < mix.size(); ++frame) {
            if (frame < voice.startSample) {
                continue;
            }
            const double t = (static_cast<double>(frame - voice.startSample)) / kSampleRate;
            const double env = sampler_adsr(t,
                                            static_cast<double>(voice.envelope.attack),
                                            static_cast<double>(voice.envelope.decay),
                                            static_cast<double>(voice.envelope.sustain),
                                            static_cast<double>(voice.envelope.release),
                                            sustain_hold);
            if (env <= 0.0) {
                continue;
            }
            const double tone_blend = static_cast<double>(voice.tone);
            const double fundamental = std::sin(2.0 * M_PI * freq * t);
            const double harmonic = std::sin(2.0 * M_PI * freq * 2.03 * t);
            double sample = (1.0 - tone_blend) * fundamental + tone_blend * harmonic;
            if (voice.usesSdStreaming) {
                const double grit = std::sin(2.0 * M_PI * freq * 0.125 * t);
                sample = (sample * 0.9) + (grit * 0.1);
            }
            mix[frame] += sample * env * pan_gain;
        }
    }

    double max_abs = 0.0;
    for (double v : mix) {
        max_abs = std::max(max_abs, std::abs(v));
    }
    const double scale = (max_abs > 0.0) ? (0.92 / max_abs) : 0.0;

    std::vector<int16_t> samples(mix.size());
    for (std::size_t i = 0; i < mix.size(); ++i) {
        const double scaled = mix[i] * scale;
        samples[i] = static_cast<int16_t>(std::clamp<std::int32_t>(
            static_cast<std::int32_t>(std::lround(scaled * 32767.0)), -32768, 32767));
    }
    return samples;
}

struct ResonatorVoiceSpec {
    Seed seed;
    std::uint32_t when;
};

Seed make_resonator_seed(std::uint8_t id,
                         float pitch,
                         float excite_ms,
                         float damping,
                         float brightness,
                         float feedback,
                         std::uint8_t mode,
                         std::uint8_t bank) {
    Seed seed{};
    seed.id = id;
    seed.pitch = pitch;
    seed.resonator.exciteMs = excite_ms;
    seed.resonator.damping = damping;
    seed.resonator.brightness = brightness;
    seed.resonator.feedback = feedback;
    seed.resonator.mode = mode;
    seed.resonator.bank = bank;
    seed.engine = static_cast<std::uint8_t>(Engine::Type::kResonator);
    return seed;
}

double modal_envelope(double t, double damping, double feedback) {
    const double sustain = std::clamp(0.35 + 0.45 * feedback, 0.1, 0.95);
    const double decay_hz = 0.75 + (1.5 - damping) * 1.75;
    return sustain * std::exp(-t * decay_hz);
}

std::vector<int16_t> render_resonator_fixture() {
    ResonatorBank bank;
    bank.init(ResonatorBank::Mode::kSim);
    bank.setMaxVoices(4);

    const ResonatorVoiceSpec specs[] = {
        {make_resonator_seed(10, -12.0f, 12.0f, 0.35f, 0.55f, 0.72f, 1, 0), 0u},
        {make_resonator_seed(11, -2.0f, 8.0f, 0.62f, 0.8f, 0.85f, 1, 2), 6000u},
        {make_resonator_seed(12, 5.0f, 5.0f, 0.48f, 0.45f, 0.6f, 0, 4), 12000u},
    };

    std::ostringstream control;
    control << "# resonator-tail control log" << '\n';
    control << "frames=" << kDroneFrames << " sample_rate_hz=" << static_cast<int>(kSampleRate)
            << " voices=" << std::size(specs) << '\n';
    control << "voice,when_samples,id,pitch,excite_ms,damping,brightness,feedback,mode,bank" << '\n';
    control << std::fixed << std::setprecision(4);

    for (std::size_t idx = 0; idx < std::size(specs); ++idx) {
        const auto& spec = specs[idx];
        bank.trigger(spec.seed, spec.when);
        control << idx << ',' << spec.when << ',' << spec.seed.id << ',' << spec.seed.pitch << ','
                << spec.seed.resonator.exciteMs << ',' << spec.seed.resonator.damping << ','
                << spec.seed.resonator.brightness << ',' << spec.seed.resonator.feedback << ','
                << static_cast<int>(spec.seed.resonator.mode) << ','
                << static_cast<int>(spec.seed.resonator.bank) << '\n';
    }
    (void)emit_control_log("resonator-tail", control.str());

    std::vector<double> mix(kDroneFrames, 0.0);
    for (std::uint8_t i = 0; i < ResonatorBank::kMaxVoices; ++i) {
        const auto voice = bank.voice(i);
        if (!voice.active) {
            continue;
        }
        const double burst = static_cast<double>(voice.burstGain);
        const double damping = static_cast<double>(voice.damping);
        const double feedback = static_cast<double>(voice.feedback);
        for (std::size_t frame = 0; frame < mix.size(); ++frame) {
            if (frame < voice.startSample) {
                continue;
            }
            const double t = (static_cast<double>(frame - voice.startSample)) / kSampleRate;
            const double envelope = modal_envelope(t, damping, feedback);
            if (envelope < 1e-6) {
                continue;
            }
            const double excite = std::exp(-std::max(0.0, t - static_cast<double>(voice.burstMs) / 1000.0) * 6.5);
            double sample = 0.0;
            for (std::size_t mode = 0; mode < voice.modalFrequencies.size(); ++mode) {
                const double freq = static_cast<double>(voice.modalFrequencies[mode]);
                const double gain = static_cast<double>(voice.modalGains[mode]);
                if (gain <= 0.0 || freq <= 0.0) {
                    continue;
                }
                sample += gain * std::sin(2.0 * M_PI * freq * t);
            }
            sample += 0.35 * std::sin(2.0 * M_PI * static_cast<double>(voice.frequency) * t);
            mix[frame] += sample * burst * envelope * (0.5 + 0.5 * excite);
        }
    }

    double max_abs = 0.0;
    for (double v : mix) {
        max_abs = std::max(max_abs, std::abs(v));
    }
    const double scale = (max_abs > 0.0) ? (0.9 / max_abs) : 0.0;

    std::vector<int16_t> samples(mix.size());
    for (std::size_t i = 0; i < mix.size(); ++i) {
        const double scaled = mix[i] * scale;
        samples[i] = static_cast<int16_t>(std::clamp<std::int32_t>(
            static_cast<std::int32_t>(std::lround(scaled * 32767.0)), -32768, 32767));
    }
    return samples;
}

struct GranularVoiceSpec {
    Seed seed;
    std::uint32_t when;
    double carrier_hz;
    double drift_amount;
};

Seed make_granular_seed(std::uint32_t id,
                        std::uint32_t prng,
                        float pitch,
                        float transpose,
                        float grain_ms,
                        float spray_ms,
                        float window_skew,
                        float spread,
                        GranularEngine::Source source,
                        std::uint8_t sd_slot) {
    Seed seed{};
    seed.id = id;
    seed.prng = prng;
    seed.pitch = pitch;
    seed.granular.transpose = transpose;
    seed.granular.grainSizeMs = grain_ms;
    seed.granular.sprayMs = spray_ms;
    seed.granular.windowSkew = window_skew;
    seed.granular.stereoSpread = spread;
    seed.granular.source = static_cast<std::uint8_t>(source);
    seed.granular.sdSlot = sd_slot;
    seed.engine = static_cast<std::uint8_t>(Engine::Type::kGranular);
    return seed;
}

double granular_window_shape(double normalized, double skew) {
    normalized = std::clamp(normalized, 0.0, 1.0);
    const double base = 0.5 - 0.5 * std::cos(normalized * M_PI);
    const double bias = std::clamp(skew, -1.0, 1.0);
    const double exponent = 1.0 + bias * 1.5;
    return std::pow(base, exponent);
}

double random_signed(uint32_t& state) {
    state = state * 1664525u + 1013904223u;
    const double scaled = static_cast<double>((state >> 8u) & 0x00FFFFFFu) / static_cast<double>(0x01000000u);
    return (scaled * 2.0) - 1.0;
}

std::vector<int16_t> render_granular_fixture() {
    GranularEngine engine;
    engine.init(GranularEngine::Mode::kSim);
    engine.setMaxActiveVoices(6);
    engine.armLiveInput(true);
    engine.registerSdClip(1, "clip-a.wav");
    engine.registerSdClip(2, "clip-b.wav");

    const GranularVoiceSpec specs[] = {
        {make_granular_seed(40, 0x1a2b3c4du, -5.0f, 0.0f, 95.0f, 24.0f, -0.45f, 0.35f,
                             GranularEngine::Source::kLiveInput, 0u),
         4000u, 146.83, 0.12},
        {make_granular_seed(41, 0x77773311u, 0.0f, 7.0f, 130.0f, 18.0f, 0.25f, 0.8f,
                             GranularEngine::Source::kSdClip, 1u),
         12000u, 196.0, 0.2},
        {make_granular_seed(42, 0x31337fffu, 7.0f, -5.0f, 80.0f, 12.0f, 0.65f, 0.6f,
                             GranularEngine::Source::kSdClip, 2u),
         20000u, 246.94, 0.08},
    };

    auto granular_source_label = [](GranularEngine::Source source) {
        switch (source) {
            case GranularEngine::Source::kLiveInput:
                return "live";
            case GranularEngine::Source::kSdClip:
                return "sd";
            default:
                return "unknown";
        }
    };

    std::ostringstream control;
    control << "# granular-haze control log" << '\n';
    control << "frames=" << kDroneFrames << " sample_rate_hz=" << static_cast<int>(kSampleRate)
            << " voices=" << std::size(specs) << '\n';
    control << "voice,when_samples,id,pitch,transpose,grain_ms,spray_ms,window_skew,spread,source,sd_slot,carrier_hz,drift" << '\n';
    control << std::fixed << std::setprecision(4);

    for (std::size_t idx = 0; idx < std::size(specs); ++idx) {
        const auto& spec = specs[idx];
        engine.trigger(spec.seed, spec.when);
        control << idx << ',' << spec.when << ',' << spec.seed.id << ',' << spec.seed.pitch << ','
                << spec.seed.granular.transpose << ',' << spec.seed.granular.grainSizeMs << ','
                << spec.seed.granular.sprayMs << ',' << spec.seed.granular.windowSkew << ','
                << spec.seed.granular.stereoSpread << ','
                << granular_source_label(static_cast<GranularEngine::Source>(spec.seed.granular.source))
                << ',' << static_cast<int>(spec.seed.granular.sdSlot) << ',' << specs[idx].carrier_hz
                << ',' << specs[idx].drift_amount << '\n';
    }
    (void)emit_control_log("granular-haze", control.str());

    std::vector<double> left(kDroneFrames, 0.0);
    std::vector<double> right(kDroneFrames, 0.0);

    for (std::size_t i = 0; i < std::size(specs); ++i) {
        const auto voice = engine.voice(static_cast<std::uint8_t>(i));
        if (!voice.active) {
            continue;
        }
        uint32_t rng = voice.seedPrng;
        const double base_freq = specs[i].carrier_hz * static_cast<double>(voice.playbackRate);
        const double grain_length = std::max(0.04, static_cast<double>(voice.sizeMs) / 1000.0);
        const double sustain = grain_length * 2.5;
        const double spread = static_cast<double>(voice.stereoSpread);
        for (std::size_t frame = voice.startSample;
             frame < left.size() && frame < voice.startSample + static_cast<std::size_t>((sustain + grain_length) * kSampleRate);
             ++frame) {
            const double t = (static_cast<double>(frame) - static_cast<double>(voice.startSample)) / kSampleRate;
            const double normalized = t / grain_length;
            if (normalized > 4.0) {
                break;
            }
            double envelope = granular_window_shape(normalized, static_cast<double>(voice.windowSkew));
            const double tail = std::exp(-t * (1.2 + 0.6 * (1.0 - spread)));
            envelope *= tail;
            if (envelope < 1e-5) {
                continue;
            }

            const double lfo = std::sin(2.0 * M_PI * (0.35 + 0.1 * spread) * t);
            const double wobble = 1.0 + 0.015 * lfo;
            const double jitter = specs[i].drift_amount * random_signed(rng);
            const double freq = base_freq * wobble * (1.0 + 0.01 * jitter);
            const double phase = 2.0 * M_PI * freq * t;
            double sample = std::sin(phase);
            sample += 0.45 * std::sin(phase * 1.62 + 0.3 * jitter);
            if (voice.source == GranularEngine::Source::kSdClip) {
                sample += 0.25 * std::sin(phase * 0.5 + random_signed(rng) * 0.4);
            } else {
                sample += 0.2 * std::sin(phase * 2.0 + lfo);
            }
            sample *= envelope * (0.75 + 0.25 * spread);
            left[frame] += sample * static_cast<double>(voice.leftGain);
            right[frame] += sample * static_cast<double>(voice.rightGain);
        }
    }

    double max_abs = 0.0;
    for (std::size_t i = 0; i < left.size(); ++i) {
        max_abs = std::max(max_abs, std::max(std::abs(left[i]), std::abs(right[i])));
    }
    const double scale = (max_abs > 0.0) ? (0.9 / max_abs) : 0.0;

    std::vector<int16_t> samples(left.size() * 2u);
    for (std::size_t i = 0; i < left.size(); ++i) {
        const double l = left[i] * scale;
        const double r = right[i] * scale;
        samples[2u * i] = static_cast<int16_t>(std::clamp<long>(static_cast<long>(std::lround(l * 32767.0)), -32768, 32767));
        samples[2u * i + 1u] = static_cast<int16_t>(
            std::clamp<long>(static_cast<long>(std::lround(r * 32767.0)), -32768, 32767));
    }
    return samples;
}

std::vector<int16_t> render_mixer_fixture() {
    const auto drone = make_drone();
    const auto sampler = render_sampler_fixture();
    const auto resonator = render_resonator_fixture();
    const auto granular = render_granular_fixture();

    const std::size_t frames = kDroneFrames;
    std::vector<double> left(frames, 0.0);
    std::vector<double> right(frames, 0.0);

    auto accumulate_mono = [&](const std::vector<int16_t>& mono, double left_gain, double right_gain) {
        const double scale = 1.0 / 32768.0;
        for (std::size_t i = 0; i < frames && i < mono.size(); ++i) {
            const double sample = static_cast<double>(mono[i]) * scale;
            left[i] += sample * left_gain;
            right[i] += sample * right_gain;
        }
    };

    std::ostringstream control;
    control << "# mixer-console control log" << '\n';
    control << "frames=" << frames << " sample_rate_hz=" << static_cast<int>(kSampleRate) << '\n';
    control << "mono_sources:" << '\n';

    accumulate_mono(drone, 0.5, 0.5);
    control << "  drone-intro -> L=0.50 R=0.50" << '\n';
    accumulate_mono(sampler, 0.35, 0.55);
    control << "  sampler-grains -> L=0.35 R=0.55" << '\n';
    accumulate_mono(resonator, 0.4, 0.3);
    control << "  resonator-tail -> L=0.40 R=0.30" << '\n';

    const double granular_scale = 1.0 / 32768.0;
    for (std::size_t frame = 0; frame < frames && (frame * 2u + 1u) < granular.size(); ++frame) {
        const double l = static_cast<double>(granular[2u * frame]) * granular_scale;
        const double r = static_cast<double>(granular[2u * frame + 1u]) * granular_scale;
        left[frame] += l * 0.65;
        right[frame] += r * 0.65;
    }
    control << "stereo_sources:" << '\n';
    control << "  granular-haze -> gain=0.65/0.65" << '\n';

    for (std::size_t i = 0; i < frames; ++i) {
        const double cross = 0.12 * (left[i] - right[i]);
        left[i] -= cross;
        right[i] += cross;
        left[i] = std::tanh(left[i] * 1.1);
        right[i] = std::tanh(right[i] * 1.1);
    }
    control << "bus_fx: crossfeed=0.12 saturate=1.10" << '\n';

    double max_abs = 0.0;
    for (std::size_t i = 0; i < frames; ++i) {
        max_abs = std::max(max_abs, std::max(std::abs(left[i]), std::abs(right[i])));
    }
    const double scale = (max_abs > 0.0) ? (0.92 / max_abs) : 0.0;

    std::vector<int16_t> samples(frames * 2u);
    for (std::size_t i = 0; i < frames; ++i) {
        const double l = left[i] * scale;
        const double r = right[i] * scale;
        samples[2u * i] = static_cast<int16_t>(std::clamp<long>(static_cast<long>(std::lround(l * 32767.0)), -32768, 32767));
        samples[2u * i + 1u] = static_cast<int16_t>(
            std::clamp<long>(static_cast<long>(std::lround(r * 32767.0)), -32768, 32767));
    }
    control << "normalize=0.92" << '\n';
    (void)emit_control_log("mixer-console", control.str());
    return samples;
}

std::vector<int16_t> render_quadraphonic_fixture() {
    const auto mixer = render_mixer_fixture();
    const auto granular = render_granular_fixture();
    const auto sampler = render_sampler_fixture();
    const auto resonator = render_resonator_fixture();
    const auto drone = make_drone();
    const auto reseed_a = render_reseed_variant(0xCAFEu);
    const auto reseed_b = render_reseed_variant(0xBEEFu);

    constexpr std::size_t kChannels = 4u;
    const std::size_t frames = kDroneFrames;
    std::array<std::vector<double>, kChannels> bus{};
    for (auto& lane : bus) {
        lane.assign(frames, 0.0);
    }

    auto accumulate_mono = [&](const std::vector<int16_t>& mono, const std::array<double, kChannels>& gains) {
        const double scale = 1.0 / 32768.0;
        const std::size_t limit = std::min(frames, mono.size());
        for (std::size_t i = 0; i < limit; ++i) {
            const double sample = static_cast<double>(mono[i]) * scale;
            for (std::size_t ch = 0; ch < kChannels; ++ch) {
                bus[ch][i] += sample * gains[ch];
            }
        }
    };

    auto accumulate_stereo = [&](const std::vector<int16_t>& stereo,
                                 const std::array<double, kChannels>& left_gains,
                                 const std::array<double, kChannels>& right_gains) {
        const double scale = 1.0 / 32768.0;
        const std::size_t sample_count = stereo.size() / 2u;
        const std::size_t limit = std::min(frames, sample_count);
        for (std::size_t i = 0; i < limit; ++i) {
            const double left = static_cast<double>(stereo[2u * i]) * scale;
            const double right = static_cast<double>(stereo[2u * i + 1u]) * scale;
            for (std::size_t ch = 0; ch < kChannels; ++ch) {
                bus[ch][i] += left * left_gains[ch];
                bus[ch][i] += right * right_gains[ch];
            }
        }
    };

    std::ostringstream control;
    control << "# quad-bus control log" << '\n';
    control << "frames=" << frames << " sample_rate_hz=" << static_cast<int>(kSampleRate) << '\n';
    control << "mono routing:" << '\n';
    control << "  drone-intro -> [0.25,0.25,0.10,0.10]" << '\n';
    control << "  sampler-grains -> [0.20,0.35,0.30,0.18]" << '\n';
    control << "  resonator-tail -> [0.32,0.22,0.14,0.36]" << '\n';
    control << "  reseed-A -> [0.30,0.18,0.42,0.24]" << '\n';
    control << "  reseed-B -> [0.18,0.30,0.24,0.46]" << '\n';
    control << "stereo routing:" << '\n';
    control << "  granular-haze -> L[0.28,0.10,0.26,-0.18] R[0.10,0.28,-0.18,0.26]" << '\n';
    control << "  mixer-console -> L[0.48,0.16,0.20,0.20] R[0.16,0.48,0.20,0.20]" << '\n';

    accumulate_mono(drone, {0.25, 0.25, 0.10, 0.10});
    accumulate_mono(sampler, {0.20, 0.35, 0.30, 0.18});
    accumulate_mono(resonator, {0.32, 0.22, 0.14, 0.36});
    accumulate_stereo(granular, {0.28, 0.10, 0.26, -0.18}, {0.10, 0.28, -0.18, 0.26});
    accumulate_stereo(mixer, {0.48, 0.16, 0.20, 0.20}, {0.16, 0.48, 0.20, 0.20});
    accumulate_mono(reseed_a, {0.30, 0.18, 0.42, 0.24});
    accumulate_mono(reseed_b, {0.18, 0.30, 0.24, 0.46});

    for (std::size_t i = 0; i < frames; ++i) {
        const double front_mid = 0.5 * (bus[0][i] + bus[1][i]);
        const double front_side = bus[0][i] - bus[1][i];
        const double rear_mid = 0.5 * (bus[2][i] + bus[3][i]);

        bus[2][i] += front_side * 0.55;
        bus[3][i] -= front_side * 0.55;
        const double tilt = 0.12 * (rear_mid - front_mid);
        bus[0][i] -= tilt;
        bus[1][i] += tilt;

        for (std::size_t ch = 0; ch < kChannels; ++ch) {
            bus[ch][i] = std::tanh(bus[ch][i] * 1.08);
        }
    }

    double max_abs = 0.0;
    for (std::size_t ch = 0; ch < kChannels; ++ch) {
        for (double sample : bus[ch]) {
            max_abs = std::max(max_abs, std::abs(sample));
        }
    }
    const double scale = (max_abs > 0.0) ? (0.92 / max_abs) : 0.0;

    std::vector<int16_t> samples(frames * kChannels);
    for (std::size_t i = 0; i < frames; ++i) {
        for (std::size_t ch = 0; ch < kChannels; ++ch) {
            const double value = bus[ch][i] * scale;
            const auto quantized = static_cast<long>(std::lround(value * 32767.0));
            samples[kChannels * i + ch] = static_cast<int16_t>(
                std::clamp<long>(quantized, -32768L, 32767L));
        }
    }
    control << "bus_fx:" << '\n';
    control << "  front_side_feed=0.55" << '\n';
    control << "  tilt=0.12" << '\n';
    control << "  saturate=1.08" << '\n';
    control << "normalize=0.92" << '\n';
    (void)emit_control_log("quad-bus", control.str());
    return samples;
}

std::vector<int16_t> render_surround_fixture() {
    const auto mixer = render_mixer_fixture();
    const auto granular = render_granular_fixture();
    const auto sampler = render_sampler_fixture();
    const auto resonator = render_resonator_fixture();
    const auto drone = make_drone();
    const auto reseed_a = render_reseed_variant(0xCAFEu);
    const auto reseed_b = render_reseed_variant(0xBEEFu);

    constexpr std::size_t kChannels = 6u;
    const std::size_t frames = kDroneFrames;
    std::array<std::vector<double>, kChannels> bus{};
    for (auto& lane : bus) {
        lane.assign(frames, 0.0);
    }

    auto accumulate_mono = [&](const std::vector<int16_t>& mono, const std::array<double, kChannels>& gains) {
        const double scale = 1.0 / 32768.0;
        const std::size_t limit = std::min(frames, mono.size());
        for (std::size_t i = 0; i < limit; ++i) {
            const double sample = static_cast<double>(mono[i]) * scale;
            for (std::size_t ch = 0; ch < kChannels; ++ch) {
                bus[ch][i] += sample * gains[ch];
            }
        }
    };

    auto accumulate_stereo = [&](const std::vector<int16_t>& stereo,
                                 const std::array<double, kChannels>& left_gains,
                                 const std::array<double, kChannels>& right_gains) {
        const double scale = 1.0 / 32768.0;
        const std::size_t sample_count = stereo.size() / 2u;
        const std::size_t limit = std::min(frames, sample_count);
        for (std::size_t i = 0; i < limit; ++i) {
            const double left = static_cast<double>(stereo[2u * i]) * scale;
            const double right = static_cast<double>(stereo[2u * i + 1u]) * scale;
            for (std::size_t ch = 0; ch < kChannels; ++ch) {
                bus[ch][i] += left * left_gains[ch];
                bus[ch][i] += right * right_gains[ch];
            }
        }
    };

    std::ostringstream control;
    control << "# surround-bus control log" << '\n';
    control << "frames=" << frames << " sample_rate_hz=" << static_cast<int>(kSampleRate) << '\n';
    control << "channel_order=[L,R,C,LFE,SL,SR]" << '\n';
    control << "mono routing:" << '\n';
    control << "  drone-intro -> [0.20,0.20,0.35,0.40,0.05,0.05]" << '\n';
    control << "  sampler-grains -> [0.32,0.42,0.28,0.10,0.18,0.24]" << '\n';
    control << "  resonator-tail -> [0.24,0.18,0.15,0.08,0.32,0.28]" << '\n';
    control << "  reseed-A -> [0.36,0.22,0.12,0.05,0.34,0.26]" << '\n';
    control << "  reseed-B -> [0.22,0.36,0.12,0.05,0.26,0.34]" << '\n';
    control << "stereo routing:" << '\n';
    control << "  granular-haze -> L[0.40,0.10,0.18,0.08,0.30,-0.08] R[0.10,0.40,0.18,0.08,-0.08,0.30]" << '\n';
    control << "  mixer-console -> L[0.55,0.20,0.12,0.05,0.16,0.06] R[0.20,0.55,0.12,0.05,0.06,0.16]" << '\n';

    accumulate_mono(drone, {0.20, 0.20, 0.35, 0.40, 0.05, 0.05});
    accumulate_mono(sampler, {0.32, 0.42, 0.28, 0.10, 0.18, 0.24});
    accumulate_mono(resonator, {0.24, 0.18, 0.15, 0.08, 0.32, 0.28});
    accumulate_stereo(granular, {0.40, 0.10, 0.18, 0.08, 0.30, -0.08}, {0.10, 0.40, 0.18, 0.08, -0.08, 0.30});
    accumulate_stereo(mixer, {0.55, 0.20, 0.12, 0.05, 0.16, 0.06}, {0.20, 0.55, 0.12, 0.05, 0.06, 0.16});
    accumulate_mono(reseed_a, {0.36, 0.22, 0.12, 0.05, 0.34, 0.26});
    accumulate_mono(reseed_b, {0.22, 0.36, 0.12, 0.05, 0.26, 0.34});

    control << "mid_side_fx: front->C+LFE ms=[0.65,0.35] surrounds_sidefeed=0.30" << '\n';
    control << "rear_glue: crossfeed=0.18 saturate=1.06" << '\n';

    for (std::size_t i = 0; i < frames; ++i) {
        const double front_mid = 0.5 * (bus[0][i] + bus[1][i]);
        const double front_side = 0.5 * (bus[0][i] - bus[1][i]);
        const double surround_mid = 0.5 * (bus[4][i] + bus[5][i]);
        const double surround_side = 0.5 * (bus[4][i] - bus[5][i]);

        bus[2][i] += front_mid * 0.65;
        bus[3][i] += 0.35 * front_mid + 0.25 * std::abs(front_side);

        bus[4][i] = surround_mid + surround_side + front_side * 0.30;
        bus[5][i] = surround_mid - surround_side - front_side * 0.30;

        const double rear_cross = 0.18 * (bus[4][i] - bus[5][i]);
        bus[4][i] -= rear_cross;
        bus[5][i] += rear_cross;

        for (std::size_t ch = 0; ch < kChannels; ++ch) {
            bus[ch][i] = std::tanh(bus[ch][i] * 1.06);
        }
    }

    double max_abs = 0.0;
    for (std::size_t ch = 0; ch < kChannels; ++ch) {
        for (double sample : bus[ch]) {
            max_abs = std::max(max_abs, std::abs(sample));
        }
    }
    const double scale = (max_abs > 0.0) ? (0.92 / max_abs) : 0.0;

    std::vector<int16_t> samples(frames * kChannels);
    for (std::size_t i = 0; i < frames; ++i) {
        for (std::size_t ch = 0; ch < kChannels; ++ch) {
            const double value = bus[ch][i] * scale;
            const auto quantized = static_cast<long>(std::lround(value * 32767.0));
            samples[kChannels * i + ch] = static_cast<int16_t>(
                std::clamp<long>(quantized, -32768L, 32767L));
        }
    }
    control << "normalize=0.92" << '\n';
    (void)emit_control_log("surround-bus", control.str());
    return samples;
}

std::string fnv1a_bytes(const std::string& bytes) {
    constexpr std::uint64_t kOffset = 1469598103934665603ull;
    constexpr std::uint64_t kPrime = 1099511628211ull;
    std::uint64_t state = kOffset;
    for (unsigned char c : bytes) {
        state ^= static_cast<std::uint64_t>(c);
        state *= kPrime;
    }
    std::ostringstream oss;
    oss << std::hex << std::nouppercase << std::setfill('0') << std::setw(16) << state;
    return oss.str();
}

std::vector<int16_t> render_long_random_take_fixture() {
    constexpr double kDurationSeconds = 30.0;
    constexpr std::uint32_t kMasterSeed = 0x30F00Du;
    constexpr int kBpm = 120;
    constexpr int kPasses = 10;

    std::vector<reseed::StemDefinition> stems = reseed::defaultStems();
    stems.push_back({"tape rattle", 4, reseed::EngineKind::kSampler});
    stems.push_back({"clank shimmer", 5, reseed::EngineKind::kResonator});

    const auto plan = reseed::makeBouncePlan(stems, kMasterSeed, kSampleRate, kBpm, kPasses);

    offline::RenderSettings settings;
    settings.sampleRate = kSampleRate;
    settings.frames = static_cast<std::size_t>(kDurationSeconds * kSampleRate);
    settings.samplerSustainHold = 0.35;
    settings.normalizeTarget = 0.9;

    offline::OfflineRenderer renderer(settings);
    renderer.mixSamplerEvents(plan.samplerEvents);
    renderer.mixResonatorEvents(plan.resonatorEvents);
    const auto& pcm = renderer.finalize();

    const std::size_t frames = settings.frames;
    std::ostringstream control;
    control << "# long-random-take control log" << '\n';
    control << "master_seed=0x" << std::hex << std::nouppercase << kMasterSeed << std::dec
            << " bpm=" << kBpm << " passes=" << kPasses << '\n';
    control << "event,name,lane,when_samples,seed_id,prng,engine" << '\n';
    for (std::size_t idx = 0; idx < plan.logEntries.size(); ++idx) {
        const auto& entry = plan.logEntries[idx];
        control << idx << ',' << entry.name << ',' << entry.lane << ',' << entry.whenSamples
                << ',' << entry.seedId << ',' << "0x" << std::hex << std::nouppercase << entry.prng
                << std::dec << ','
                << static_cast<int>(entry.engine == reseed::EngineKind::kResonator ? 2 : 0) << '\n';
    }
    (void)emit_control_log("long-random-take", control.str());

    std::vector<int16_t> trimmed(frames * 2u, 0);
    const std::size_t available_frames = std::min(frames, pcm.size());
    for (std::size_t i = 0; i < available_frames; ++i) {
        const int16_t sample = pcm[i];
        trimmed[2u * i] = sample;
        trimmed[2u * i + 1u] = sample;
    }
    return trimmed;
}
std::vector<int16_t> render_reseed_variant(std::uint32_t master_seed,
                                           const char* control_fixture_name) {
    const auto& stems = reseed::defaultStems();
    const auto plan = reseed::makeBouncePlan(stems, master_seed, kSampleRate, 124, 3);
    offline::OfflineRenderer renderer({kSampleRate, plan.framesHint});
    renderer.mixSamplerEvents(plan.samplerEvents);
    renderer.mixResonatorEvents(plan.resonatorEvents);
    const auto& pcm = renderer.finalize();
    if (control_fixture_name != nullptr) {
        std::ostringstream control;
        control << "# " << control_fixture_name << " control log" << '\n';
        control << "master_seed=0x" << std::hex << std::nouppercase << master_seed << std::dec
                << " bpm=124 passes=3" << '\n';
        control << "event,name,lane,when_samples,seed_id,prng,engine" << '\n';
        for (std::size_t idx = 0; idx < plan.logEntries.size(); ++idx) {
            const auto& entry = plan.logEntries[idx];
            control << idx << ',' << entry.name << ',' << entry.lane << ',' << entry.whenSamples
                    << ',' << entry.seedId << ',' << "0x" << std::hex << std::nouppercase
                    << entry.prng << std::dec << ','
                    << static_cast<int>(entry.engine == reseed::EngineKind::kResonator ? 2 : 0)
                    << '\n';
        }
        (void)emit_control_log(control_fixture_name, control.str());
    }
    return std::vector<int16_t>(pcm.begin(), pcm.end());
}

std::string render_reseed_log_fixture() {
    const auto& stems = reseed::defaultStems();
    const auto plan_a = reseed::makeBouncePlan(stems, 0xCAFEu, kSampleRate, 124, 3);
    const auto plan_b = reseed::makeBouncePlan(stems, 0xBEEFu, kSampleRate, 124, 3);

    std::vector<reseed::BounceLogBlock> blocks;
    blocks.push_back({"A", 0xCAFEu, "out/reseed-A.wav", plan_a.logEntries});
    blocks.push_back({"B", 0xBEEFu, "out/reseed-B.wav", plan_b.logEntries});

    return reseed::serializeEventLog(stems, blocks, kSampleRate, 124, 3);
}

std::string render_euclid_log() {
    EuclidEngine engine;
    Engine::PrepareContext prep{};
    prep.masterSeed = 0xE0C10BADu;
    engine.prepare(prep);

    engine.onParam({0, static_cast<std::uint16_t>(EuclidEngine::Param::kSteps), 16});
    engine.onParam({0, static_cast<std::uint16_t>(EuclidEngine::Param::kFills), 5});
    engine.onParam({0, static_cast<std::uint16_t>(EuclidEngine::Param::kRotate), 3});

    Seed seed{};
    seed.id = 24;
    engine.onSeed({seed, 0u});

    std::ostringstream log;
    log << "# Euclid engine mask" << '\n';
    log << "steps=" << static_cast<int>(engine.mask().size())
        << " fills=5 rotate=3 master_seed=0x" << std::hex << std::nouppercase << engine.generationSeed()
        << std::dec << '\n';
    log << "mask:";
    for (std::size_t i = 0; i < engine.mask().size(); ++i) {
        log << (engine.mask()[i] ? 'X' : '.');
    }
    log << '\n';

    log << "gates:";
    Engine::TickContext tick{};
    const std::size_t sample_span = engine.mask().empty() ? 0 : engine.mask().size() * 2;
    for (std::size_t step = 0; step < sample_span; ++step) {
        tick.tick = step;
        engine.onTick(tick);
        log << (engine.lastGate() ? '1' : '0');
    }
    log << '\n';
    return log.str();
}

std::string render_burst_log() {
    BurstEngine engine;
    Engine::PrepareContext prep{};
    prep.masterSeed = 0xB0057BADu;
    engine.prepare(prep);

    engine.onParam({0, static_cast<std::uint16_t>(BurstEngine::Param::kClusterCount), 6});
    engine.onParam({0, static_cast<std::uint16_t>(BurstEngine::Param::kSpacingSamples), 720});

    Seed seed{};
    seed.id = 77;
    engine.onSeed({seed, 2048u});

    std::ostringstream log;
    log << "# Burst engine trigger cluster" << '\n';
    log << "cluster_count=6 spacing_samples=720 master_seed=0x" << std::hex << std::nouppercase
        << engine.generationSeed() << std::dec << '\n';
    log << "pending:";
    const auto& pending = engine.pendingTriggers();
    for (std::size_t i = 0; i < pending.size(); ++i) {
        if (i != 0) {
            log << ',';
        }
        log << pending[i];
    }
    log << '\n';
    return log.str();
}

bool write_text_file(const std::filesystem::path& path, const std::string& body) {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) {
        return false;
    }
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        return false;
    }
    out.write(body.data(), static_cast<std::streamsize>(body.size()));
    return out.good();
}


std::filesystem::path project_root() {
    return std::filesystem::current_path();
}

std::filesystem::path fixture_path(const char* manifest_path) {
    std::filesystem::path path(manifest_path);
    if (path.is_absolute()) {
        return path;
    }
    if (path.generic_string().rfind("build/fixtures", 0) == 0) {
        return project_root() / path;
    }
    return project_root() / "build/fixtures" / path;
}

bool emit_control_log(const char* fixture_name, const std::string& body) {
    if (fixture_name == nullptr || *fixture_name == '\0') {
        return false;
    }
    const std::string manifest = std::string("build/fixtures/") + fixture_name + "-control.txt";
    return write_text_file(fixture_path(manifest.c_str()), body);
}

void ensure_fixture(const GoldenFixture& spec,
                    const std::vector<int16_t>& samples,
                    std::uint16_t channels) {
    golden::WavWriteRequest request{};
    request.path = fixture_path(spec.path).string();
    request.sample_rate_hz = static_cast<std::uint32_t>(kSampleRate);
    request.channels = channels;
    request.samples = samples;

    if (!golden::write_wav_16(request)) {
        throw std::runtime_error(std::string("Failed to write fixture ") + spec.name);
    }
    const auto hash = golden::hash_pcm16(request.samples);
    if (hash != spec.expected_hash) {
        throw std::runtime_error(std::string("Hash mismatch for ") + spec.name +
                                 ": expected " + spec.expected_hash + " got " + hash);
    }
    std::cout << "[ok] " << spec.name << " -> " << spec.path << " (" << hash << ")" << std::endl;
}

void ensure_log_fixture(const GoldenFixture& spec, const std::string& body) {
    const auto path = fixture_path(spec.path);
    if (!write_text_file(path, body)) {
        throw std::runtime_error(std::string("Failed to write log fixture ") + spec.name);
    }
    const auto hash = fnv1a_bytes(body);
    if (hash != spec.expected_hash) {
        throw std::runtime_error(std::string("Log hash mismatch for ") + spec.name +
                                 ": expected " + spec.expected_hash + " got " + hash);
    }
    std::cout << "[ok] " << spec.name << " -> " << spec.path << " (" << hash << ")" << std::endl;
}

template <typename Generator>
void maybe_emit_audio(const GoldenFixture& spec,
                      std::uint16_t channels,
                      Generator&& generator,
                      const std::vector<std::string>& filters) {
    if (!should_emit(spec, FixtureKind::kAudio, filters)) {
        std::cout << "[skip] " << spec.name << " (filtered)" << std::endl;
        return;
    }
    ensure_fixture(spec, generator(), channels);
}

template <typename Generator>
void maybe_emit_log(const GoldenFixture& spec,
                    Generator&& generator,
                    const std::vector<std::string>& filters) {
    if (!should_emit(spec, FixtureKind::kLog, filters)) {
        std::cout << "[skip] " << spec.name << " (filtered)" << std::endl;
        return;
    }
    ensure_log_fixture(spec, generator());
}

}  // namespace

int main() {
    try {
        const auto root = project_root() / "build/fixtures";
        std::filesystem::create_directories(root);

        const auto filters = parse_filter_tokens();
        if (!filters.empty()) {
            std::cout << "[offline-native-golden] active filters:";
            for (const auto& token : filters) {
                std::cout << ' ' << token;
            }
            std::cout << std::endl;
        }

        maybe_emit_audio(kAudioFixtures[0], 1, [] { return make_drone(); }, filters);
        maybe_emit_audio(kAudioFixtures[1], 1, [] { return render_sampler_fixture(); }, filters);
        maybe_emit_audio(kAudioFixtures[2], 1, [] { return render_resonator_fixture(); }, filters);
        maybe_emit_audio(kAudioFixtures[3], 2, [] { return render_granular_fixture(); }, filters);
        maybe_emit_audio(kAudioFixtures[4], 2, [] { return render_mixer_fixture(); }, filters);
        maybe_emit_audio(kAudioFixtures[5], 4, [] { return render_quadraphonic_fixture(); }, filters);
        maybe_emit_audio(kAudioFixtures[6], 6, [] { return render_surround_fixture(); }, filters);
        maybe_emit_audio(kAudioFixtures[7], 1, [] { return render_long_random_take_fixture(); }, filters);
        maybe_emit_audio(kAudioFixtures[8], 1,
                        [] { return render_reseed_variant(0xCAFEu, "reseed-A"); }, filters);
        maybe_emit_audio(kAudioFixtures[9], 1,
                        [] { return render_reseed_variant(0xBEEFu, "reseed-B"); }, filters);

        maybe_emit_log(kLogFixtures[0], [] { return render_euclid_log(); }, filters);
        maybe_emit_log(kLogFixtures[1], [] { return render_burst_log(); }, filters);
        maybe_emit_log(kLogFixtures[2], [] { return render_reseed_log_fixture(); }, filters);

        std::cout << "Native golden fixtures refreshed." << std::endl;
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Native golden fixture refresh failed: " << ex.what() << std::endl;
        return 1;
    }
}


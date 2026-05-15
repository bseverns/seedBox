#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "app/AppState.h"
#include "util/RNG.h"
#include "hal/Board.h"
#include "hal/hal_audio.h"

namespace {

struct ProbeConfig {
  std::filesystem::path inputPath;
  std::filesystem::path outputPath;
  std::filesystem::path statusPath;
  std::filesystem::path summaryPath;
  std::string scenario = "mixed-boot";
  std::string clockMode = "internal-block";
  float bpm = 120.0f;
  std::uint32_t targetSampleRate = 48000u;
  std::size_t blockSize = 256u;
};

struct AudioFile {
  std::uint32_t sampleRate = 0u;
  std::uint16_t channels = 0u;
  std::vector<float> left;
  std::vector<float> right;
};

struct SignalStats {
  double peak = 0.0;
  double rms = 0.0;
  double meanAbs = 0.0;
};

struct BlockRmsStats {
  std::size_t blocks = 0u;
  double min = 0.0;
  double max = 0.0;
  double mean = 0.0;
  double stddev = 0.0;
};

struct ProbeSummary {
  SignalStats inputStats{};
  SignalStats outputLeftStats{};
  SignalStats outputRightStats{};
  double outputInputMeanAbsDiff = 0.0;
  std::uint64_t inputFrames = 0u;
  std::uint64_t resampledFrames = 0u;
  std::uint64_t renderedFrames = 0u;
};

struct ScenarioRuntime {
  std::uint64_t blocksRendered = 0u;
  std::uint64_t reseedCount = 0u;
  std::uint64_t nextReseedFrame = 0u;
  std::uint32_t reseedState = 0xB4000001u;
  double ppqnSamplesAccum = 0.0;
};

struct ScenarioSpec {
  const char* name;
  const char* category;
  const char* note;
  void (*setup)(AppState& app, ScenarioRuntime& runtime);
  void (*beforeBlock)(AppState& app, ScenarioRuntime& runtime, std::uint64_t frameOffset, const ProbeConfig& config);
};

std::uint16_t readU16(std::istream& in) {
  std::array<unsigned char, 2> bytes{};
  in.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  if (!in) {
    throw std::runtime_error("unexpected EOF while reading u16");
  }
  return static_cast<std::uint16_t>(bytes[0] | (static_cast<std::uint16_t>(bytes[1]) << 8u));
}

std::uint32_t readU32(std::istream& in) {
  std::array<unsigned char, 4> bytes{};
  in.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  if (!in) {
    throw std::runtime_error("unexpected EOF while reading u32");
  }
  return static_cast<std::uint32_t>(bytes[0] | (static_cast<std::uint32_t>(bytes[1]) << 8u) |
                                    (static_cast<std::uint32_t>(bytes[2]) << 16u) |
                                    (static_cast<std::uint32_t>(bytes[3]) << 24u));
}

void expectTag(std::istream& in, const char* tag) {
  char got[4]{};
  in.read(got, 4);
  if (!in || std::string_view(got, 4) != std::string_view(tag, 4)) {
    throw std::runtime_error(std::string("expected WAV tag ") + tag);
  }
}

AudioFile readPcm16Wave(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    throw std::runtime_error("failed to open input WAV");
  }

  expectTag(in, "RIFF");
  (void)readU32(in);
  expectTag(in, "WAVE");

  std::uint16_t channels = 0u;
  std::uint32_t sampleRate = 0u;
  std::uint16_t bitsPerSample = 0u;
  std::vector<std::int16_t> pcm{};

  while (in) {
    char chunkIdChars[4]{};
    in.read(chunkIdChars, 4);
    if (!in) {
      break;
    }
    const std::string chunkId(chunkIdChars, 4);
    const std::uint32_t chunkSize = readU32(in);

    if (chunkId == "fmt ") {
      const std::uint16_t audioFormat = readU16(in);
      channels = readU16(in);
      sampleRate = readU32(in);
      (void)readU32(in);
      (void)readU16(in);
      bitsPerSample = readU16(in);
      if (audioFormat != 1u) {
        throw std::runtime_error("only PCM WAV files are supported");
      }
      if (chunkSize > 16u) {
        in.seekg(static_cast<std::streamoff>(chunkSize - 16u), std::ios::cur);
      }
    } else if (chunkId == "data") {
      if (chunkSize % sizeof(std::int16_t) != 0u) {
        throw std::runtime_error("PCM16 data chunk had odd byte count");
      }
      pcm.resize(chunkSize / sizeof(std::int16_t));
      in.read(reinterpret_cast<char*>(pcm.data()), static_cast<std::streamsize>(chunkSize));
      if (!in) {
        throw std::runtime_error("failed to read WAV payload");
      }
    } else {
      in.seekg(static_cast<std::streamoff>(chunkSize), std::ios::cur);
    }

    if ((chunkSize & 1u) != 0u) {
      in.seekg(1, std::ios::cur);
    }
  }

  if (channels == 0u || sampleRate == 0u || bitsPerSample != 16u || pcm.empty()) {
    throw std::runtime_error("unsupported or incomplete WAV file");
  }
  if (channels > 2u) {
    throw std::runtime_error("only mono and stereo WAV files are supported");
  }
  if (pcm.size() % channels != 0u) {
    throw std::runtime_error("PCM payload does not divide evenly by channel count");
  }

  AudioFile audio{};
  audio.sampleRate = sampleRate;
  audio.channels = channels;
  const std::size_t frames = pcm.size() / channels;
  audio.left.resize(frames, 0.0f);
  if (channels > 1u) {
    audio.right.resize(frames, 0.0f);
  }

  for (std::size_t frame = 0; frame < frames; ++frame) {
    audio.left[frame] = static_cast<float>(pcm[frame * channels]) / 32768.0f;
    if (channels > 1u) {
      audio.right[frame] = static_cast<float>(pcm[frame * channels + 1u]) / 32768.0f;
    }
  }
  return audio;
}

std::vector<float> resampleLinear(const std::vector<float>& in, std::uint32_t srcRate, std::uint32_t dstRate) {
  if (in.empty() || srcRate == 0u || dstRate == 0u) {
    return {};
  }
  if (srcRate == dstRate) {
    return in;
  }

  const double ratio = static_cast<double>(dstRate) / static_cast<double>(srcRate);
  const std::size_t outFrames = static_cast<std::size_t>(std::llround(static_cast<double>(in.size()) * ratio));
  std::vector<float> out(outFrames, 0.0f);
  for (std::size_t i = 0; i < outFrames; ++i) {
    const double srcPos = static_cast<double>(i) / ratio;
    const std::size_t idx0 = static_cast<std::size_t>(std::floor(srcPos));
    const std::size_t idx1 = std::min(idx0 + 1u, in.size() - 1u);
    const double frac = srcPos - static_cast<double>(idx0);
    out[i] = static_cast<float>((1.0 - frac) * static_cast<double>(in[idx0]) +
                                frac * static_cast<double>(in[idx1]));
  }
  return out;
}

SignalStats computeStats(const std::vector<float>& samples) {
  SignalStats stats{};
  if (samples.empty()) {
    return stats;
  }
  double sumSq = 0.0;
  double sumAbs = 0.0;
  for (float sample : samples) {
    const double value = static_cast<double>(sample);
    const double absValue = std::abs(value);
    stats.peak = std::max(stats.peak, absValue);
    sumSq += value * value;
    sumAbs += absValue;
  }
  stats.rms = std::sqrt(sumSq / static_cast<double>(samples.size()));
  stats.meanAbs = sumAbs / static_cast<double>(samples.size());
  return stats;
}

double computeMeanAbsDiff(const std::vector<float>& leftA, const std::vector<float>& rightA,
                          const std::vector<float>& leftB, const std::vector<float>& rightB) {
  const std::size_t frames = std::min({leftA.size(), rightA.size(), leftB.size(), rightB.size()});
  if (frames == 0u) {
    return 0.0;
  }

  double diffSum = 0.0;
  for (std::size_t i = 0; i < frames; ++i) {
    diffSum += std::abs(static_cast<double>(leftA[i]) - static_cast<double>(leftB[i]));
    diffSum += std::abs(static_cast<double>(rightA[i]) - static_cast<double>(rightB[i]));
  }
  return diffSum / static_cast<double>(frames * 2u);
}

std::uint64_t countClipRiskSamples(const std::vector<float>& left, const std::vector<float>& right) {
  const std::size_t frames = std::min(left.size(), right.size());
  std::uint64_t count = 0u;
  for (std::size_t i = 0; i < frames; ++i) {
    if (std::abs(left[i]) >= 0.999f) {
      ++count;
    }
    if (std::abs(right[i]) >= 0.999f) {
      ++count;
    }
  }
  return count;
}

BlockRmsStats computeBlockRmsStats(const std::vector<float>& left, const std::vector<float>& right,
                                   std::size_t blockSize) {
  BlockRmsStats stats{};
  const std::size_t frames = std::min(left.size(), right.size());
  if (frames == 0u || blockSize == 0u) {
    return stats;
  }

  std::vector<double> blockRms{};
  blockRms.reserve((frames + blockSize - 1u) / blockSize);
  for (std::size_t offset = 0; offset < frames; offset += blockSize) {
    const std::size_t end = std::min(offset + blockSize, frames);
    double sumSq = 0.0;
    for (std::size_t i = offset; i < end; ++i) {
      const double leftValue = static_cast<double>(left[i]);
      const double rightValue = static_cast<double>(right[i]);
      sumSq += (leftValue * leftValue) + (rightValue * rightValue);
    }
    const double denom = static_cast<double>((end - offset) * 2u);
    blockRms.push_back(std::sqrt(sumSq / denom));
  }

  stats.blocks = blockRms.size();
  stats.min = *std::min_element(blockRms.begin(), blockRms.end());
  stats.max = *std::max_element(blockRms.begin(), blockRms.end());
  double sum = 0.0;
  for (const double value : blockRms) {
    sum += value;
  }
  stats.mean = sum / static_cast<double>(blockRms.size());
  double sumSqDiff = 0.0;
  for (const double value : blockRms) {
    const double diff = value - stats.mean;
    sumSqDiff += diff * diff;
  }
  stats.stddev = std::sqrt(sumSqDiff / static_cast<double>(blockRms.size()));
  return stats;
}

std::int16_t clampToPcm16(float sample) {
  const float clamped = std::max(-1.0f, std::min(1.0f, sample));
  return static_cast<std::int16_t>(std::lrint(clamped * 32767.0f));
}

void writePcm16Wave(const std::filesystem::path& path, const std::vector<std::int16_t>& interleaved,
                    std::uint32_t sampleRate, std::uint16_t channels) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary);
  if (!out) {
    throw std::runtime_error("failed to create output WAV");
  }

  const std::uint32_t dataBytes = static_cast<std::uint32_t>(interleaved.size() * sizeof(std::int16_t));
  const std::uint32_t byteRate = sampleRate * channels * 2u;
  const std::uint16_t blockAlign = static_cast<std::uint16_t>(channels * 2u);

  auto writeTag = [&](const char* tag) { out.write(tag, 4); };
  auto writeU16 = [&](std::uint16_t value) {
    out.put(static_cast<char>(value & 0xFFu));
    out.put(static_cast<char>((value >> 8u) & 0xFFu));
  };
  auto writeU32 = [&](std::uint32_t value) {
    out.put(static_cast<char>(value & 0xFFu));
    out.put(static_cast<char>((value >> 8u) & 0xFFu));
    out.put(static_cast<char>((value >> 16u) & 0xFFu));
    out.put(static_cast<char>((value >> 24u) & 0xFFu));
  };

  writeTag("RIFF");
  writeU32(36u + dataBytes);
  writeTag("WAVE");
  writeTag("fmt ");
  writeU32(16u);
  writeU16(1u);
  writeU16(channels);
  writeU32(sampleRate);
  writeU32(byteRate);
  writeU16(blockAlign);
  writeU16(16u);
  writeTag("data");
  writeU32(dataBytes);
  out.write(reinterpret_cast<const char*>(interleaved.data()), static_cast<std::streamsize>(dataBytes));
  if (!out) {
    throw std::runtime_error("failed to finish output WAV");
  }
}

std::string jsonEscape(const std::string& text) {
  std::string out;
  out.reserve(text.size() + 16u);
  for (char ch : text) {
    switch (ch) {
      case '\\':
        out += "\\\\";
        break;
      case '"':
        out += "\\\"";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        out += ch;
        break;
    }
  }
  return out;
}

void writeTextFile(const std::filesystem::path& path, const std::string& body) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream out(path);
  if (!out) {
    throw std::runtime_error("failed to write text artifact");
  }
  out << body;
}

bool usesExternalPpqnClock(const ProbeConfig& config) {
  if (config.clockMode == "external-ppqn") {
    return true;
  }
  if (config.clockMode == "internal-block") {
    return false;
  }
  throw std::runtime_error("unknown clock mode: " + config.clockMode);
}

void requireSeedEdit(AppState& app, std::uint8_t seedIndex, const std::function<void(Seed&)>& edit) {
  if (!app.applySeedEditFromHost(seedIndex, edit)) {
    throw std::runtime_error("failed to edit scenario seed state");
  }
}

void quietSeed(AppState& app, std::uint8_t seedIndex) {
  requireSeedEdit(app, seedIndex, [](Seed& seed) {
    seed.density = 0.0f;
    seed.probability = 0.0f;
    seed.mutateAmt = 0.0f;
    seed.jitterMs = 0.0f;
    seed.spread = 0.0f;
    seed.tone = 0.25f;
  });
}

void configureSeedPrimeWorld(AppState& app, std::uint32_t masterSeed, AppState::SeedPrimeMode mode) {
  app.seedPageReseed(masterSeed, mode);
  app.armGranularLiveInput(true);
  app.setFocusSeed(0);
}

void configureGranularLiveSeed(AppState& app) {
  app.setSeedEngine(0, EngineRouter::kGranularId);
  requireSeedEdit(app, 0, [](Seed& seed) {
    seed.pitch = 12.0f;
    seed.density = 4.1f;
    seed.probability = 1.0f;
    seed.mutateAmt = 1.0f;
    seed.tone = 0.94f;
    seed.spread = 1.0f;
    seed.granular.grainSizeMs = 340.0f;
    seed.granular.sprayMs = 96.0f;
    seed.granular.transpose = 12.0f;
    seed.granular.windowSkew = 0.82f;
    seed.granular.stereoSpread = 1.0f;
    seed.granular.source = static_cast<std::uint8_t>(GranularEngine::Source::kLiveInput);
    seed.granular.sdSlot = 0;
  });
}

void configureResonatorLiveSeed(AppState& app) {
  app.setSeedEngine(0, EngineRouter::kResonatorId);
  requireSeedEdit(app, 0, [](Seed& seed) {
    seed.density = 1.4f;
    seed.probability = 0.9f;
    seed.mutateAmt = 0.4f;
    seed.tone = 0.7f;
    seed.spread = 0.76f;
    seed.pitch = 7.0f;
    seed.resonator.exciteMs = 42.0f;
    seed.resonator.damping = 0.48f;
    seed.resonator.brightness = 0.78f;
    seed.resonator.feedback = 0.72f;
    seed.resonator.bank = 1;
  });
}

void configureBurstLiveSeed(AppState& app) {
  app.setSeedEngine(0, EngineRouter::kBurstId);
  requireSeedEdit(app, 0, [](Seed& seed) {
    seed.pitch = 7.0f;
    seed.density = 4.4f;
    seed.probability = 1.0f;
    seed.jitterMs = 42.0f;
    seed.mutateAmt = 1.0f;
    seed.tone = 1.0f;
    seed.spread = 0.08f;
  });
}

void configureEuclidLiveSeed(AppState& app) {
  app.setSeedEngine(0, EngineRouter::kEuclidId);
  requireSeedEdit(app, 0, [](Seed& seed) {
    seed.pitch = -5.0f;
    seed.density = 3.0f;
    seed.probability = 1.0f;
    seed.mutateAmt = 0.92f;
    seed.tone = 1.0f;
    seed.spread = 0.0f;
  });
}

void configureSupportGranularLayer(AppState& app) {
  app.setSeedEngine(1, EngineRouter::kGranularId);
  requireSeedEdit(app, 1, [](Seed& seed) {
    seed.pitch = 0.0f;
    seed.density = 1.1f;
    seed.probability = 0.7f;
    seed.mutateAmt = 0.35f;
    seed.tone = 0.5f;
    seed.spread = 0.9f;
    seed.granular.grainSizeMs = 110.0f;
    seed.granular.sprayMs = 18.0f;
    seed.granular.transpose = 0.0f;
    seed.granular.windowSkew = 0.1f;
    seed.granular.stereoSpread = 0.75f;
    seed.granular.source = static_cast<std::uint8_t>(GranularEngine::Source::kLiveInput);
    seed.granular.sdSlot = 0;
  });
}

void scenarioNoopBeforeBlock(AppState&, ScenarioRuntime&, std::uint64_t, const ProbeConfig&) {}

void setupMixedBoot(AppState& app, ScenarioRuntime&) {
  app.reseed(app.masterSeed());
  app.armGranularLiveInput(true);
  app.setFocusSeed(0);
}

void setupGranularLive(AppState& app, ScenarioRuntime&) {
  configureSeedPrimeWorld(app, 0xB4001001u, AppState::SeedPrimeMode::kLiveInput);
  configureGranularLiveSeed(app);
  quietSeed(app, 1);
  quietSeed(app, 2);
  quietSeed(app, 3);
}

void setupResonatorLive(AppState& app, ScenarioRuntime&) {
  configureSeedPrimeWorld(app, 0xB4002001u, AppState::SeedPrimeMode::kLiveInput);
  configureResonatorLiveSeed(app);
  quietSeed(app, 1);
  quietSeed(app, 2);
  quietSeed(app, 3);
}

void setupBurstOverlay(AppState& app, ScenarioRuntime&) {
  configureSeedPrimeWorld(app, 0xB4003001u, AppState::SeedPrimeMode::kLiveInput);
  configureBurstLiveSeed(app);
  configureSupportGranularLayer(app);
  app.setFocusSeed(0);
  quietSeed(app, 2);
  quietSeed(app, 3);
}

void setupEuclidOverlay(AppState& app, ScenarioRuntime&) {
  configureSeedPrimeWorld(app, 0xB4004001u, AppState::SeedPrimeMode::kLiveInput);
  configureEuclidLiveSeed(app);
  configureSupportGranularLayer(app);
  app.setFocusSeed(0);
  quietSeed(app, 2);
  quietSeed(app, 3);
}

void setupReseedLive(AppState& app, ScenarioRuntime& runtime) {
  configureSeedPrimeWorld(app, 0xB4005001u, AppState::SeedPrimeMode::kLiveInput);
  configureGranularLiveSeed(app);
  quietSeed(app, 1);
  quietSeed(app, 2);
  quietSeed(app, 3);
  runtime.reseedState = 0xB4005001u;
  runtime.nextReseedFrame = 0u;
}

void configureReseedVoiceWorld(AppState& app, ScenarioRuntime& runtime) {
  switch (runtime.reseedCount % 4u) {
    case 0u:
      configureGranularLiveSeed(app);
      break;
    case 1u:
      configureResonatorLiveSeed(app);
      break;
    case 2u:
      configureBurstLiveSeed(app);
      break;
    case 3u:
    default:
      configureEuclidLiveSeed(app);
      break;
  }
  app.setFocusSeed(0);
}

void beforeBlockReseedLive(AppState& app, ScenarioRuntime& runtime, std::uint64_t frameOffset, const ProbeConfig& config) {
  const std::uint64_t reseedIntervalFrames = static_cast<std::uint64_t>(config.targetSampleRate) * 8u;
  while (frameOffset >= runtime.nextReseedFrame) {
    if (runtime.reseedState == 0u) {
      runtime.reseedState = 0xB4005001u;
    }
    RNG::xorshift(runtime.reseedState);
    app.seedPageReseed(runtime.reseedState, AppState::SeedPrimeMode::kLiveInput);
    configureReseedVoiceWorld(app, runtime);
    ++runtime.reseedCount;
    runtime.nextReseedFrame += reseedIntervalFrames;
  }
}

const ScenarioSpec& lookupScenario(std::string_view name) {
  // Scenario setup functions live here, while user-facing metadata is mirrored in
  // docs/fixtures/external_input_scenarios.json for Python tooling. Keep this
  // table in sync by running scripts/validate_input_scenarios.py after edits.
  static const std::array<ScenarioSpec, 6> kScenarios{{
      {"mixed-boot", "hybrid-overlay",
       "Desktop boot preset with granular live-input focus plus resonator, burst, and Euclid scheduler voices.",
       &setupMixedBoot, &scenarioNoopBeforeBlock},
      {"granular-live", "direct-input-processor",
       "Focused granular live-input render. The file is the effect material, not just a trigger source.",
       &setupGranularLive, &scenarioNoopBeforeBlock},
      {"resonator-live", "direct-input-processor",
       "Focused resonator live-input render. The file excites the resonant body directly.",
       &setupResonatorLive, &scenarioNoopBeforeBlock},
      {"burst-overlay", "scheduler-overlay",
       "Burst-focused live-input render with a lighter granular support layer.",
       &setupBurstOverlay, &scenarioNoopBeforeBlock},
      {"euclid-overlay", "direct-input-processor",
       "Euclid-focused live-input render with a lighter granular support layer.",
       &setupEuclidOverlay, &scenarioNoopBeforeBlock},
      {"reseed-live", "direct-input-reseed",
       "Live-input render with deterministic periodic reseeds that cycle the focused processor through granular, resonator, burst, and Euclid worlds.",
       &setupReseedLive, &beforeBlockReseedLive},
  }};

  for (const auto& scenario : kScenarios) {
    if (name == scenario.name) {
      return scenario;
    }
  }
  throw std::runtime_error("unknown scenario: " + std::string(name));
}

ProbeConfig parseArgs(int argc, char** argv) {
  ProbeConfig config{};
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    auto requireValue = [&](const char* flag) -> std::string {
      if (i + 1 >= argc) {
        throw std::runtime_error(std::string(flag) + " requires a value");
      }
      return argv[++i];
    };

    if (arg == "--input") {
      config.inputPath = requireValue("--input");
    } else if (arg == "--output") {
      config.outputPath = requireValue("--output");
    } else if (arg == "--status-json") {
      config.statusPath = requireValue("--status-json");
    } else if (arg == "--summary-json") {
      config.summaryPath = requireValue("--summary-json");
    } else if (arg == "--scenario") {
      config.scenario = requireValue("--scenario");
    } else if (arg == "--clock-mode") {
      config.clockMode = requireValue("--clock-mode");
    } else if (arg == "--bpm") {
      config.bpm = std::stof(requireValue("--bpm"));
    } else if (arg == "--block-size") {
      config.blockSize = static_cast<std::size_t>(std::stoul(requireValue("--block-size")));
    } else if (arg == "--sample-rate") {
      config.targetSampleRate = static_cast<std::uint32_t>(std::stoul(requireValue("--sample-rate")));
    } else {
      throw std::runtime_error("unknown argument: " + arg);
    }
  }

  if (config.inputPath.empty() || config.outputPath.empty() || config.statusPath.empty() || config.summaryPath.empty()) {
    throw std::runtime_error("usage: seedbox_native_input_probe --input <wav> --output <wav> "
                             "--status-json <json> --summary-json <json> [--scenario name] "
                             "[--clock-mode internal-block|external-ppqn] [--bpm BPM] "
                             "[--block-size N] [--sample-rate Hz]");
  }
  return config;
}

}  // namespace

int main(int argc, char** argv) {
  try {
    const ProbeConfig config = parseArgs(argc, argv);
    const ScenarioSpec& scenario = lookupScenario(config.scenario);
    const bool externalPpqnClock = usesExternalPpqnClock(config);
    AudioFile input = readPcm16Wave(config.inputPath);
    AudioFile resampled{};
    resampled.sampleRate = config.targetSampleRate;
    resampled.channels = input.channels;
    resampled.left = resampleLinear(input.left, input.sampleRate, config.targetSampleRate);
    if (input.channels > 1u) {
      resampled.right = resampleLinear(input.right, input.sampleRate, config.targetSampleRate);
    } else {
      resampled.right = resampled.left;
    }

    AppState app(hal::board());
    app.initJuceHost(static_cast<float>(config.targetSampleRate), config.blockSize);
    ScenarioRuntime scenarioRuntime{};
    scenario.setup(app, scenarioRuntime);
    app.setInternalBpmFromHost(config.bpm);
    if (externalPpqnClock) {
      app.setFollowExternalClockFromHost(true);
      app.onExternalTransportStart();
    }

    std::vector<float> outLeft(resampled.left.size(), 0.0f);
    std::vector<float> outRight(resampled.left.size(), 0.0f);
    std::vector<float> blockLeft(config.blockSize, 0.0f);
    std::vector<float> blockRight(config.blockSize, 0.0f);
    std::vector<float> renderLeft(config.blockSize, 0.0f);
    std::vector<float> renderRight(config.blockSize, 0.0f);
    const double samplesPerPpqnTick =
        static_cast<double>(config.targetSampleRate) * 60.0 / (static_cast<double>(config.bpm) * 24.0);

    constexpr std::uint64_t kMaintenanceEveryBlocks = 16u;
    for (std::size_t offset = 0; offset < resampled.left.size(); offset += config.blockSize) {
      const std::size_t frames = std::min(config.blockSize, resampled.left.size() - offset);
      scenario.beforeBlock(app, scenarioRuntime, static_cast<std::uint64_t>(offset), config);
      std::fill(blockLeft.begin(), blockLeft.end(), 0.0f);
      std::fill(blockRight.begin(), blockRight.end(), 0.0f);
      std::copy_n(resampled.left.data() + offset, frames, blockLeft.data());
      if (input.channels > 1u) {
        std::copy_n(resampled.right.data() + offset, frames, blockRight.data());
        app.setDryInputFromHost(blockLeft.data(), blockRight.data(), frames);
      } else {
        app.setDryInputFromHost(blockLeft.data(), nullptr, frames);
      }

      hal::audio::renderHostBuffer(renderLeft.data(), renderRight.data(), frames);
      app.midi.poll();
      if (externalPpqnClock) {
        scenarioRuntime.ppqnSamplesAccum += static_cast<double>(frames);
        while (scenarioRuntime.ppqnSamplesAccum >= samplesPerPpqnTick) {
          app.onExternalClockTick();
          scenarioRuntime.ppqnSamplesAccum -= samplesPerPpqnTick;
        }
      }
      app.tickHostAudio();
      std::copy_n(renderLeft.data(), frames, outLeft.data() + offset);
      std::copy_n(renderRight.data(), frames, outRight.data() + offset);
      ++scenarioRuntime.blocksRendered;
      if ((scenarioRuntime.blocksRendered % kMaintenanceEveryBlocks) == 0u) {
        app.serviceHostMaintenance();
      }
    }

    if (externalPpqnClock) {
      app.onExternalTransportStop();
    }
    app.serviceHostMaintenance();

    std::vector<std::int16_t> interleaved{};
    interleaved.reserve(outLeft.size() * 2u);
    for (std::size_t i = 0; i < outLeft.size(); ++i) {
      interleaved.push_back(clampToPcm16(outLeft[i]));
      interleaved.push_back(clampToPcm16(outRight[i]));
    }
    writePcm16Wave(config.outputPath, interleaved, config.targetSampleRate, 2u);

    const SignalStats sourceInputStats = computeStats(input.left);
    const SignalStats resampledInputStats = computeStats(resampled.left);
    const SignalStats outputLeftStats = computeStats(outLeft);
    const SignalStats outputRightStats = computeStats(outRight);
    const double outputMaxPeak = std::max(outputLeftStats.peak, outputRightStats.peak);
    const double stereoMeanAbsDiff = computeMeanAbsDiff(outLeft, outLeft, outRight, outRight);
    const double inputOutputMeanAbsDiff = computeMeanAbsDiff(outLeft, outRight, resampled.left, resampled.right);
    const std::uint64_t clippedSampleCount = countClipRiskSamples(outLeft, outRight);
    const BlockRmsStats outputBlockRmsStats = computeBlockRmsStats(outLeft, outRight, config.blockSize);

    double diffSum = 0.0;
    for (std::size_t i = 0; i < resampled.left.size(); ++i) {
      diffSum += std::abs(static_cast<double>(outLeft[i]) - static_cast<double>(resampled.left[i]));
    }

    const std::string statusJson = app.captureStatusJson();
    writeTextFile(config.statusPath, statusJson + "\n");

    std::ostringstream summary;
    summary << std::fixed << std::setprecision(6);
    summary << "{\n";
    summary << "  \"scenario\": {\n";
    summary << "    \"name\": \"" << jsonEscape(scenario.name) << "\",\n";
    summary << "    \"category\": \"" << jsonEscape(scenario.category) << "\",\n";
    summary << "    \"note\": \"" << jsonEscape(scenario.note) << "\",\n";
    summary << "    \"reseedCount\": " << scenarioRuntime.reseedCount << "\n";
    summary << "  },\n";
    summary << "  \"clock\": {\n";
    summary << "    \"mode\": \"" << jsonEscape(config.clockMode) << "\",\n";
    summary << "    \"bpm\": " << config.bpm << ",\n";
    summary << "    \"ppqn\": 24\n";
    summary << "  },\n";
    summary << "  \"input\": {\n";
    summary << "    \"path\": \"" << jsonEscape(config.inputPath.string()) << "\",\n";
    summary << "    \"sourceSampleRate\": " << input.sampleRate << ",\n";
    summary << "    \"sourceChannels\": " << input.channels << ",\n";
    summary << "    \"sourceFrames\": " << input.left.size() << ",\n";
    summary << "    \"resampledFrames\": " << resampled.left.size() << ",\n";
    summary << "    \"targetSampleRate\": " << config.targetSampleRate << ",\n";
    summary << "    \"sourcePeak\": " << sourceInputStats.peak << ",\n";
    summary << "    \"sourceRms\": " << sourceInputStats.rms << ",\n";
    summary << "    \"resampledPeak\": " << resampledInputStats.peak << ",\n";
    summary << "    \"resampledRms\": " << resampledInputStats.rms << "\n";
    summary << "  },\n";
    summary << "  \"render\": {\n";
    summary << "    \"blockSize\": " << config.blockSize << ",\n";
    summary << "    \"outputPath\": \"" << jsonEscape(config.outputPath.string()) << "\",\n";
    summary << "    \"frames\": " << outLeft.size() << ",\n";
    summary << "    \"leftPeak\": " << outputLeftStats.peak << ",\n";
    summary << "    \"leftRms\": " << outputLeftStats.rms << ",\n";
    summary << "    \"rightPeak\": " << outputRightStats.peak << ",\n";
    summary << "    \"rightRms\": " << outputRightStats.rms << ",\n";
    summary << "    \"maxPeak\": " << outputMaxPeak << ",\n";
    summary << "    \"clippedSampleCount\": " << clippedSampleCount << ",\n";
    summary << "    \"stereoMeanAbsDiff\": " << stereoMeanAbsDiff << ",\n";
    summary << "    \"inputOutputMeanAbsDiff\": " << inputOutputMeanAbsDiff << ",\n";
    summary << "    \"blockRms\": {\n";
    summary << "      \"blocks\": " << outputBlockRmsStats.blocks << ",\n";
    summary << "      \"min\": " << outputBlockRmsStats.min << ",\n";
    summary << "      \"max\": " << outputBlockRmsStats.max << ",\n";
    summary << "      \"mean\": " << outputBlockRmsStats.mean << ",\n";
    summary << "      \"stddev\": " << outputBlockRmsStats.stddev << ",\n";
    summary << "      \"range\": " << (outputBlockRmsStats.max - outputBlockRmsStats.min) << "\n";
    summary << "    },\n";
    summary << "    \"leftInputMeanAbsDiff\": "
            << (resampled.left.empty() ? 0.0 : diffSum / static_cast<double>(resampled.left.size())) << "\n";
    summary << "  },\n";
    summary << "  \"statusJsonPath\": \"" << jsonEscape(config.statusPath.string()) << "\"\n";
    summary << "}\n";
    writeTextFile(config.summaryPath, summary.str());

    std::cout << "Rendered " << outLeft.size() << " frames at " << config.targetSampleRate << " Hz\n";
    std::cout << "Scenario: " << scenario.name << "\n";
    std::cout << "Output WAV: " << config.outputPath << "\n";
    std::cout << "Status JSON: " << config.statusPath << "\n";
    std::cout << "Summary JSON: " << config.summaryPath << "\n";
    return 0;
  } catch (const std::exception& ex) {
    std::cerr << "seedbox_native_input_probe: " << ex.what() << std::endl;
    return 1;
  }
}

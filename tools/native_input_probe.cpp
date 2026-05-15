#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "app/AppState.h"
#include "hal/Board.h"
#include "hal/hal_audio.h"

namespace {

struct ProbeConfig {
  std::filesystem::path inputPath;
  std::filesystem::path outputPath;
  std::filesystem::path statusPath;
  std::filesystem::path summaryPath;
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

struct ProbeSummary {
  SignalStats inputStats{};
  SignalStats outputLeftStats{};
  SignalStats outputRightStats{};
  double outputInputMeanAbsDiff = 0.0;
  std::uint64_t inputFrames = 0u;
  std::uint64_t resampledFrames = 0u;
  std::uint64_t renderedFrames = 0u;
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
                             "--status-json <json> --summary-json <json> [--block-size N] [--sample-rate Hz]");
  }
  return config;
}

}  // namespace

int main(int argc, char** argv) {
  try {
    const ProbeConfig config = parseArgs(argc, argv);
    AudioFile input = readPcm16Wave(config.inputPath);
    AudioFile resampled{};
    resampled.sampleRate = config.targetSampleRate;
    resampled.channels = input.channels;
    resampled.left = resampleLinear(input.left, input.sampleRate, config.targetSampleRate);
    if (input.channels > 1u) {
      resampled.right = resampleLinear(input.right, input.sampleRate, config.targetSampleRate);
    }

    AppState app(hal::board());
    app.initJuceHost(static_cast<float>(config.targetSampleRate), config.blockSize);

    std::vector<float> outLeft(resampled.left.size(), 0.0f);
    std::vector<float> outRight(resampled.left.size(), 0.0f);
    std::vector<float> blockLeft(config.blockSize, 0.0f);
    std::vector<float> blockRight(config.blockSize, 0.0f);
    std::vector<float> renderLeft(config.blockSize, 0.0f);
    std::vector<float> renderRight(config.blockSize, 0.0f);

    for (std::size_t offset = 0; offset < resampled.left.size(); offset += config.blockSize) {
      const std::size_t frames = std::min(config.blockSize, resampled.left.size() - offset);
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
      std::copy_n(renderLeft.data(), frames, outLeft.data() + offset);
      std::copy_n(renderRight.data(), frames, outRight.data() + offset);
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

    double diffSum = 0.0;
    for (std::size_t i = 0; i < resampled.left.size(); ++i) {
      diffSum += std::abs(static_cast<double>(outLeft[i]) - static_cast<double>(resampled.left[i]));
    }

    const std::string statusJson = app.captureStatusJson();
    writeTextFile(config.statusPath, statusJson + "\n");

    std::ostringstream summary;
    summary << std::fixed << std::setprecision(6);
    summary << "{\n";
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
    summary << "    \"leftInputMeanAbsDiff\": "
            << (resampled.left.empty() ? 0.0 : diffSum / static_cast<double>(resampled.left.size())) << "\n";
    summary << "  },\n";
    summary << "  \"statusJsonPath\": \"" << jsonEscape(config.statusPath.string()) << "\"\n";
    summary << "}\n";
    writeTextFile(config.summaryPath, summary.str());

    std::cout << "Rendered " << outLeft.size() << " frames at " << config.targetSampleRate << " Hz\n";
    std::cout << "Output WAV: " << config.outputPath << "\n";
    std::cout << "Status JSON: " << config.statusPath << "\n";
    std::cout << "Summary JSON: " << config.summaryPath << "\n";
    return 0;
  } catch (const std::exception& ex) {
    std::cerr << "seedbox_native_input_probe: " << ex.what() << std::endl;
    return 1;
  }
}

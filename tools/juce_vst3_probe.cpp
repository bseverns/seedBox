#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr double kDefaultSampleRate = 48000.0;
constexpr int kDefaultBlockSize = 512;
constexpr double kDefaultRenderSeconds = 3.0;
constexpr int kOutputChannels = 2;
constexpr float kSilencePeakFloor = 1.0e-4f;
constexpr double kSilenceRmsFloor = 1.0e-5;

struct ProbeConfig {
  juce::File pluginPath;
  juce::File outputPath;
  double sampleRate{kDefaultSampleRate};
  int blockSize{kDefaultBlockSize};
  double renderSeconds{kDefaultRenderSeconds};
};

struct RenderStats {
  float peak{0.0f};
  double rms{0.0};
};

juce::String usageText() {
  return "Usage: seedbox_juce_vst3_probe [--plugin /path/to/SeedBox.vst3] "
         "[--output /path/to/seedbox_probe.wav] [--seconds 3] "
         "[--sample-rate 48000] [--block-size 512]";
}

ProbeConfig makeDefaultConfig() {
  ProbeConfig config;
  const juce::File cwd = juce::File::getCurrentWorkingDirectory();

  const std::vector<juce::File> pluginCandidates = {
      juce::File::getSpecialLocation(juce::File::userHomeDirectory)
          .getChildFile("Library/Audio/Plug-Ins/VST3/SeedBox.vst3"),
      cwd.getChildFile("build/juce-arm64-fx/SeedboxVST3_artefacts/VST3/SeedBox.vst3"),
      cwd.getChildFile("build/juce/SeedboxVST3_artefacts/VST3/SeedBox.vst3"),
  };

  for (const auto& candidate : pluginCandidates) {
    if (candidate.exists()) {
      config.pluginPath = candidate;
      break;
    }
  }

  config.outputPath = cwd.getChildFile("build/juce_probe/seedbox_vst3_probe.wav");
  return config;
}

ProbeConfig parseArgs(int argc, char* argv[]) {
  ProbeConfig config = makeDefaultConfig();

  for (int i = 1; i < argc; ++i) {
    const juce::String arg(argv[i]);

    auto requireValue = [&](const juce::String& flag) -> juce::String {
      if (i + 1 >= argc) {
        throw std::runtime_error((flag + " requires a value").toStdString());
      }
      return juce::String(argv[++i]);
    };

    if (arg == "--plugin") {
      config.pluginPath = juce::File(requireValue(arg));
    } else if (arg == "--output") {
      config.outputPath = juce::File(requireValue(arg));
    } else if (arg == "--seconds") {
      config.renderSeconds = requireValue(arg).getDoubleValue();
    } else if (arg == "--sample-rate") {
      config.sampleRate = requireValue(arg).getDoubleValue();
    } else if (arg == "--block-size") {
      config.blockSize = requireValue(arg).getIntValue();
    } else if (arg == "--help" || arg == "-h") {
      std::cout << usageText() << '\n';
      std::exit(0);
    } else {
      throw std::runtime_error(("unknown argument: " + arg).toStdString());
    }
  }

  if (!config.pluginPath.exists()) {
    throw std::runtime_error("SeedBox.vst3 not found; pass --plugin with a valid bundle path");
  }
  if (config.renderSeconds <= 0.0) {
    throw std::runtime_error("--seconds must be > 0");
  }
  if (config.sampleRate <= 0.0) {
    throw std::runtime_error("--sample-rate must be > 0");
  }
  if (config.blockSize <= 0) {
    throw std::runtime_error("--block-size must be > 0");
  }

  return config;
}

juce::PluginDescription scanVst3Description(const juce::File& pluginPath) {
  juce::VST3PluginFormatHeadless format;
  juce::OwnedArray<juce::PluginDescription> descriptions;
  format.findAllTypesForFile(descriptions, pluginPath.getFullPathName());

  if (descriptions.isEmpty()) {
    throw std::runtime_error("failed to scan VST3 bundle for plugin descriptions");
  }

  return *descriptions[0];
}

void configureBuses(juce::AudioPluginInstance& plugin) {
  plugin.enableAllBuses();

  auto layout = plugin.getBusesLayout();
  if (plugin.getBusCount(true) > 0) {
    layout.inputBuses.getReference(0) = juce::AudioChannelSet::disabled();
  }
  if (plugin.getBusCount(false) > 0) {
    layout.outputBuses.getReference(0) = juce::AudioChannelSet::stereo();
  }

  if (!plugin.setBusesLayout(layout)) {
    throw std::runtime_error("plugin rejected probe bus layout");
  }
}

juce::String listParameterNames(juce::AudioPluginInstance& plugin) {
  juce::StringArray names;
  for (auto* parameter : plugin.getParameters()) {
    if (parameter == nullptr) {
      continue;
    }
    names.add(parameter->getName(128));
  }
  return names.joinIntoString(", ");
}

bool setBoolParameter(juce::AudioPluginInstance& plugin,
                      const juce::String& id,
                      const juce::String& fallbackName,
                      bool enabled) {
  for (auto* parameter : plugin.getParameters()) {
    if (parameter == nullptr) {
      continue;
    }

    if (auto* parameterWithId = dynamic_cast<juce::AudioProcessorParameterWithID*>(parameter)) {
      if (parameterWithId->paramID == id) {
        parameterWithId->beginChangeGesture();
        parameterWithId->setValueNotifyingHost(enabled ? 1.0f : 0.0f);
        parameterWithId->endChangeGesture();
        return true;
      }
    }

    if (parameter->getName(128) == fallbackName) {
      parameter->beginChangeGesture();
      parameter->setValueNotifyingHost(enabled ? 1.0f : 0.0f);
      parameter->endChangeGesture();
      return true;
    }
  }

  return false;
}

RenderStats computeStats(const juce::AudioBuffer<float>& buffer) {
  RenderStats stats;
  double sumSquares = 0.0;
  const int numChannels = buffer.getNumChannels();
  const int numSamples = buffer.getNumSamples();

  for (int ch = 0; ch < numChannels; ++ch) {
    const float* channel = buffer.getReadPointer(ch);
    for (int i = 0; i < numSamples; ++i) {
      const float sample = channel[i];
      stats.peak = std::max(stats.peak, std::abs(sample));
      sumSquares += static_cast<double>(sample) * static_cast<double>(sample);
    }
  }

  const double denom = static_cast<double>(std::max(1, numChannels * numSamples));
  stats.rms = std::sqrt(sumSquares / denom);
  return stats;
}

void writeWav(const juce::File& outputPath, const juce::AudioBuffer<float>& buffer, double sampleRate) {
  outputPath.getParentDirectory().createDirectory();

  std::unique_ptr<juce::OutputStream> outputStream(outputPath.createOutputStream().release());
  if (outputStream == nullptr) {
    throw std::runtime_error("failed to create output wav stream");
  }

  juce::WavAudioFormat wav;
  auto options = juce::AudioFormatWriter::Options()
                     .withSampleRate(sampleRate)
                     .withNumChannels(buffer.getNumChannels())
                     .withBitsPerSample(24);

  auto writer = wav.createWriterFor(outputStream, options);
  if (writer == nullptr) {
    throw std::runtime_error("failed to create wav writer");
  }

  if (!writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples())) {
    throw std::runtime_error("failed to write rendered audio to wav");
  }
}

juce::AudioBuffer<float> renderProbe(juce::AudioPluginInstance& plugin,
                                     double sampleRate,
                                     int blockSize,
                                     double seconds) {
  const int totalSamples = static_cast<int>(std::ceil(seconds * sampleRate));
  juce::AudioBuffer<float> rendered(kOutputChannels, totalSamples);
  rendered.clear();

  juce::AudioBuffer<float> blockBuffer(kOutputChannels, blockSize);
  juce::MidiBuffer midi;

  plugin.setRateAndBufferSizeDetails(sampleRate, blockSize);
  plugin.prepareToPlay(sampleRate, blockSize);

  int offset = 0;
  while (offset < totalSamples) {
    const int chunk = std::min(blockSize, totalSamples - offset);
    blockBuffer.clear();

    if (chunk < blockSize) {
      blockBuffer.setSize(kOutputChannels, chunk, true, true, true);
    }

    plugin.processBlock(blockBuffer, midi);

    for (int ch = 0; ch < rendered.getNumChannels(); ++ch) {
      rendered.copyFrom(ch, offset, blockBuffer, ch, 0, chunk);
    }

    if (chunk != blockSize) {
      blockBuffer.setSize(kOutputChannels, blockSize, true, true, true);
    }

    offset += chunk;
  }

  plugin.releaseResources();
  return rendered;
}

}  // namespace

int main(int argc, char* argv[]) {
  try {
    const juce::ScopedJuceInitialiser_GUI juceInit;
    const ProbeConfig config = parseArgs(argc, argv);

    const juce::PluginDescription description = scanVst3Description(config.pluginPath);
    juce::AudioPluginFormatManager formatManager;
    juce::addHeadlessDefaultFormatsToManager(formatManager);

    juce::String errorMessage;
    auto plugin = formatManager.createPluginInstance(description,
                                                     config.sampleRate,
                                                     config.blockSize,
                                                     errorMessage);
    if (plugin == nullptr) {
      throw std::runtime_error(("failed to instantiate plugin: " + errorMessage).toStdString());
    }

    configureBuses(*plugin);

    if (!setBoolParameter(*plugin, "testTone", "Test Tone", true)) {
      throw std::runtime_error(("failed to find VST3 test tone parameter; available parameters: " +
                                listParameterNames(*plugin))
                                   .toStdString());
    }

    auto rendered = renderProbe(*plugin, config.sampleRate, config.blockSize, config.renderSeconds);
    const RenderStats stats = computeStats(rendered);

    if (stats.peak < kSilencePeakFloor && stats.rms < kSilenceRmsFloor) {
      throw std::runtime_error("render completed but output looked silent");
    }

    writeWav(config.outputPath, rendered, config.sampleRate);

    std::cout << "Probe OK\n";
    std::cout << "plugin=" << config.pluginPath.getFullPathName() << '\n';
    std::cout << "output=" << config.outputPath.getFullPathName() << '\n';
    std::cout << "seconds=" << config.renderSeconds << '\n';
    std::cout << "sample_rate=" << config.sampleRate << '\n';
    std::cout << "block_size=" << config.blockSize << '\n';
    std::cout << "peak=" << stats.peak << '\n';
    std::cout << "rms=" << stats.rms << '\n';
    return 0;
  } catch (const std::exception& ex) {
    std::cerr << "Probe failed: " << ex.what() << '\n';
    std::cerr << usageText() << '\n';
    return 1;
  }
}

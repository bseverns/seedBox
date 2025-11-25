#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "Seed.h"
#include "engine/Engine.h"

#include "../shared/offline_renderer.hpp"

namespace {

struct Options {
  bool exportWav = false;
  bool quietExport = false;
  int grains = 18;
  double sprayMs = 22.0;
  std::string wavPath = "out/live-grains.wav";
};

Options parseArgs(int argc, char** argv) {
  Options opts;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--export-wav") {
      opts.exportWav = true;
    } else if (arg.rfind("--export-wav=", 0) == 0) {
      opts.exportWav = true;
      opts.wavPath = arg.substr(std::string("--export-wav=").size());
    } else if (arg == "--quiet-export") {
      opts.quietExport = true;
    } else if (arg.rfind("--grains=", 0) == 0) {
      opts.grains = std::max(1, std::stoi(arg.substr(std::string("--grains=").size())));
    } else if (arg.rfind("--spray-ms=", 0) == 0) {
      opts.sprayMs = std::max(0.0, std::stod(arg.substr(std::string("--spray-ms=").size())));
    } else if (arg == "--help") {
      std::cout << "Usage: program [--export-wav[=path]] [--quiet-export] [--grains=N] [--spray-ms=MS]\n";
      std::exit(0);
    }
  }
  return opts;
}

struct GrainPlan {
  offline::SamplerEvent sampler;
  bool doubleDip = false;
};

Seed makeSamplerSeed(std::uint32_t id, float pitch, float toneTilt, float spread) {
  Seed seed{};
  seed.id = id;
  seed.lineage = 0xC0FFEEu;
  seed.engine = static_cast<std::uint8_t>(Engine::Type::kSampler);
  seed.sampleIdx = static_cast<std::uint8_t>(id % 6);
  seed.pitch = pitch;
  seed.tone = toneTilt;
  seed.spread = spread;
  seed.envA = 0.004f;
  seed.envD = 0.12f;
  seed.envS = 0.58f;
  seed.envR = 0.22f;
  seed.jitterMs = 0.0f;
  seed.density = 0.9f;
  seed.probability = 0.95f;
  seed.granular.source = 0;  // mirrors live input path in sim
  seed.granular.grainSizeMs = 85.f;
  seed.granular.sprayMs = 18.f;
  seed.granular.transpose = 0.0f;
  return seed;
}

Seed makeResonatorSeed(std::uint32_t id, float pitch) {
  Seed seed{};
  seed.id = id;
  seed.lineage = 0xB00Fu;
  seed.engine = static_cast<std::uint8_t>(Engine::Type::kResonator);
  seed.pitch = pitch;
  seed.resonator.exciteMs = 4.5f;
  seed.resonator.damping = 0.32f;
  seed.resonator.brightness = 0.55f;
  seed.resonator.feedback = 0.65f;
  seed.resonator.mode = static_cast<std::uint8_t>(id % 2);
  seed.resonator.bank = static_cast<std::uint8_t>((id / 3) % 5);
  return seed;
}

std::vector<GrainPlan> scriptGrains(const Options& opts, double framesPerBeat) {
  std::vector<GrainPlan> grains;
  grains.reserve(static_cast<std::size_t>(opts.grains));
  std::mt19937 rng(0xF00Du);
  std::normal_distribution<double> jitterMs(0.0, opts.sprayMs * 0.6);

  for (int i = 0; i < opts.grains; ++i) {
    const double beat = static_cast<double>(i) * 0.5;  // two ghost taps per beat
    const double whenMs = beat * (framesPerBeat / 48.0) + jitterMs(rng);
    const std::uint32_t whenSamples = static_cast<std::uint32_t>(
        std::max(0.0, std::round(whenMs * 48.0)));  // 48kHz -> 48 samples per ms

    const float pitch = -5.0f + static_cast<float>(i % 7) * 1.25f;
    const float toneTilt = 0.25f + static_cast<float>(i % 3) * 0.15f;
    const float spread = 0.25f + static_cast<float>((i + 2) % 5) * 0.1f;

    GrainPlan plan;
    plan.sampler = {makeSamplerSeed(100 + static_cast<std::uint32_t>(i), pitch, toneTilt, spread),
                    whenSamples};
    plan.doubleDip = (i % 4 == 0);
    grains.push_back(plan);
  }
  return grains;
}

std::vector<offline::ResonatorEvent> sketchResonatorBursts(const std::vector<GrainPlan>& grains) {
  std::vector<offline::ResonatorEvent> bursts;
  std::uint32_t id = 400;
  for (const auto& grain : grains) {
    if (!grain.doubleDip) {
      continue;
    }
    const auto when = grain.sampler.whenSamples + 480;  // kick the modal bank a hair later
    bursts.push_back({makeResonatorSeed(id++, grain.sampler.seed.pitch + 5.0f), when});
  }
  return bursts;
}

void describeGrain(const GrainPlan& plan, int index, bool quiet) {
  if (quiet) {
    return;
  }
  std::cout << "  grain " << index << " -> sampleIdx=" << static_cast<int>(plan.sampler.seed.sampleIdx)
            << ", pitch=" << plan.sampler.seed.pitch << " st, pan=" << plan.sampler.seed.spread
            << (plan.doubleDip ? " + modal echo" : "") << "\n";
}

}  // namespace

int main(int argc, char** argv) {
  const Options opts = parseArgs(argc, argv);
  constexpr double kSampleRate = 48000.0;
  constexpr double kBpm = 108.0;
  const double framesPerBeat = (kSampleRate * 60.0) / kBpm;

  if (!opts.quietExport) {
    std::cout << "[granular-live] simulating kGranular seeds as ghost sampler taps\n";
    std::cout << "  grains=" << opts.grains << ", sprayMs=" << opts.sprayMs
              << ", quiet-export=" << std::boolalpha << opts.quietExport << "\n";
  }

  const auto grains = scriptGrains(opts, framesPerBeat);
  const auto bursts = sketchResonatorBursts(grains);

  offline::OfflineRenderer renderer({kSampleRate, static_cast<std::size_t>(framesPerBeat * (opts.grains + 6))});
  std::vector<offline::SamplerEvent> samplerEvents;
  samplerEvents.reserve(grains.size());
  for (std::size_t i = 0; i < grains.size(); ++i) {
    samplerEvents.push_back(grains[i].sampler);
    describeGrain(grains[i], static_cast<int>(i), opts.quietExport);
  }
  renderer.mixSamplerEvents(samplerEvents);
  renderer.mixResonatorEvents(bursts);
  const auto& pcm = renderer.finalize();

  if (opts.exportWav) {
    if (offline::OfflineRenderer::exportWav(opts.wavPath, static_cast<std::uint32_t>(kSampleRate), pcm)) {
      if (!opts.quietExport) {
        std::cout << "[granular-live] wrote mix -> " << opts.wavPath << " (" << pcm.size() << " samples)\n";
      }
    } else {
      std::cerr << "[granular-live] failed to export WAV at " << opts.wavPath << "\n";
      return 1;
    }
  } else if (!opts.quietExport) {
    std::cout << "[granular-live] pass --export-wav to bounce into out/ without waking any DACs.\n";
  }

  if (!opts.quietExport) {
    std::cout << "[granular-live] ghost render complete." << std::endl;
  }
  return 0;
}


#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "engine/Engine.h"
#include "Seed.h"

#include "../shared/offline_renderer.hpp"

struct QuietEvent {
  std::string label;
  int beat;
  enum class Engine { kSampler, kResonator } engine{Engine::kSampler};
};

class QuietSequencer {
 public:
  void setEvents(std::vector<QuietEvent> events) { events_ = std::move(events); }

  struct RenderPlan {
    std::vector<offline::SamplerEvent> samplerEvents;
    std::vector<offline::ResonatorEvent> resonatorEvents;
    std::size_t framesHint = 0;
  };

  RenderPlan run(int measures, int bpm, bool quietMode, double sampleRate) const {
    const auto beatDuration = std::chrono::milliseconds(60000 / bpm);
    std::cout << "[sprout] quiet-mode=" << std::boolalpha << quietMode << "\n";

    const auto waitDuration = quietMode ? beatDuration / 10 : beatDuration;
    const double framesPerBeat = sampleRate * (60.0 / static_cast<double>(bpm));

    RenderPlan plan;
    double maxWhen = 0.0;
    std::uint32_t seedCounter = 1;

    for (int beat = 0; beat < measures * 4; ++beat) {
      std::this_thread::sleep_for(waitDuration);
      for (const auto &event : events_) {
        if (event.beat == beat % 4) {
          const double when = framesPerBeat * static_cast<double>(beat);
          const auto whenSamples = static_cast<std::uint32_t>(std::lround(when));
          const Seed seed = makeSeed(event, seedCounter++);
          if (event.engine == QuietEvent::Engine::kSampler) {
            plan.samplerEvents.push_back({seed, whenSamples});
          } else {
            plan.resonatorEvents.push_back({seed, whenSamples});
          }
          maxWhen = std::max(maxWhen, static_cast<double>(whenSamples));
          std::cout << "  • ghosting " << event.label << " @beat " << beat;
          if (event.engine == QuietEvent::Engine::kResonator) {
            std::cout << " (resonator ping)";
          }
          std::cout << "\n";
        }
      }
    }
    std::cout << "[sprout] finished without touching the DAC." << std::endl;
    const double totalBeats = static_cast<double>(measures * 4);
    const double baseFrames = framesPerBeat * totalBeats;
    const double tail = sampleRate * 2.5;
    plan.framesHint = static_cast<std::size_t>(std::lround(std::max(baseFrames + sampleRate, maxWhen + tail)));
    return plan;
  }

 private:
  static Seed makeSamplerSeed(const std::string &label, std::uint32_t id) {
    Seed seed{};
    seed.id = id;
    seed.engine = static_cast<std::uint8_t>(Engine::Type::kSampler);
    seed.sampleIdx = 0;
    seed.pitch = -5.0f;
    seed.envA = 0.005f;
    seed.envD = 0.09f;
    seed.envS = 0.55f;
    seed.envR = 0.18f;
    seed.tone = 0.35f;
    seed.spread = 0.1f;
    seed.mutateAmt = 0.05f;

    if (label.find("snare") != std::string::npos) {
      seed.sampleIdx = 1;
      seed.pitch = -1.0f;
      seed.envA = 0.004f;
      seed.envD = 0.12f;
      seed.envS = 0.4f;
      seed.envR = 0.22f;
      seed.tone = 0.65f;
      seed.spread = 0.35f;
    } else if (label.find("hat") != std::string::npos || label.find("ghost") != std::string::npos) {
      seed.sampleIdx = 2;
      seed.pitch = 7.0f;
      seed.envA = 0.0025f;
      seed.envD = 0.06f;
      seed.envS = 0.3f;
      seed.envR = 0.14f;
      seed.tone = 0.55f;
      seed.spread = 0.6f;
    }
    return seed;
  }

  static Seed makeResonatorSeed(const std::string &label, std::uint32_t id) {
    Seed seed{};
    seed.id = id;
    seed.engine = static_cast<std::uint8_t>(Engine::Type::kResonator);
    seed.pitch = label.find("hat") != std::string::npos ? 12.0f : -3.0f;
    seed.resonator.exciteMs = label.find("hat") != std::string::npos ? 6.5f : 4.0f;
    seed.resonator.damping = label.find("clap") != std::string::npos ? 0.42f : 0.5f;
    seed.resonator.brightness = label.find("hat") != std::string::npos ? 0.72f : 0.58f;
    seed.resonator.feedback = label.find("clap") != std::string::npos ? 0.74f : 0.62f;
    seed.resonator.mode = label.find("hat") != std::string::npos ? 1 : 0;
    seed.resonator.bank = label.find("hat") != std::string::npos ? 1 : 3;
    return seed;
  }

  static Seed makeSeed(const QuietEvent &event, std::uint32_t id) {
    if (event.engine == QuietEvent::Engine::kResonator) {
      return makeResonatorSeed(event.label, id);
    }
    return makeSamplerSeed(event.label, id);
  }

  std::vector<QuietEvent> events_;
};

struct SproutOptions {
  bool quietMode = true;
  std::string mutation = "default";
  bool showHelp = false;
  bool listMutations = false;
  bool exportWav = false;
};

SproutOptions parseArgs(int argc, char **argv) {
  SproutOptions options;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--quiet") {
      options.quietMode = true;
    } else if (arg == "--loud" || arg == "--no-quiet") {
      options.quietMode = false;
    } else if (arg == "--export-wav") {
      options.exportWav = true;
    } else if (arg.rfind("--mutate=", 0) == 0) {
      options.mutation = arg.substr(std::string("--mutate=").size());
    } else if (arg == "--list-mutations") {
      options.listMutations = true;
    } else if (arg == "--help" || arg == "-h") {
      options.showHelp = true;
    } else {
      std::cerr << "[sprout] unknown flag: " << arg << "\n";
      options.showHelp = true;
    }
  }
  return options;
}

void printHelp() {
  std::cout << "sprout controls:\n"
            << "  --quiet          keep the sim sped-up (default)\n"
            << "  --loud           stretch beats to real-time 4/4\n"
            << "  --mutate=<name>  swap in a different ghost kit\n"
            << "  --export-wav     bounce /out/intro-sprout.wav before exit\n"
            << "  --list-mutations show the known kit options\n"
            << std::endl;
}

using MutationTable = std::vector<std::pair<std::string, std::vector<QuietEvent>>>;

const MutationTable &mutations() {
  static const MutationTable table = {
      {"default",
       {{"kick placeholder", 0, QuietEvent::Engine::kSampler},
        {"snare scribble", 2, QuietEvent::Engine::kSampler},
        {"hat rustle", 3, QuietEvent::Engine::kResonator}}},
      {"hatless",
       {{"kick placeholder", 0, QuietEvent::Engine::kSampler},
        {"snare scribble", 2, QuietEvent::Engine::kSampler}}},
      {"afterbeat-chop",
       {{"kick placeholder", 0, QuietEvent::Engine::kSampler},
        {"snare scribble", 3, QuietEvent::Engine::kSampler},
        {"ghost clap", 1, QuietEvent::Engine::kResonator}}},
  };
  return table;
}

void listMutations() {
  std::cout << "available ghost kits:\n";
  for (const auto &entry : mutations()) {
    std::cout << "  - " << entry.first << '\n';
  }
  std::cout << std::endl;
}

std::vector<QuietEvent> resolveMutation(const std::string &mutationName) {
  for (const auto &entry : mutations()) {
    if (entry.first == mutationName) {
      return entry.second;
    }
  }
  std::cerr << "[sprout] missing mutation '" << mutationName
            << "', sliding back to default." << std::endl;
  return mutations().front().second;
}

int main(int argc, char **argv) {
  const auto options = parseArgs(argc, argv);
  constexpr double kSampleRate = 48000.0;

  if (options.showHelp) {
    printHelp();
  }
  if (options.listMutations) {
    listMutations();
  }
  if (options.showHelp || options.listMutations) {
    return 0;
  }

  QuietSequencer sequencer;
  sequencer.setEvents(resolveMutation(options.mutation));

  std::cout << "[sprout] mutation=" << options.mutation << '\n';
  const auto plan = sequencer.run(/*measures=*/2, /*bpm=*/96, options.quietMode, kSampleRate);
  std::cout << "[sprout] staged " << plan.samplerEvents.size() << " sampler hits and "
            << plan.resonatorEvents.size() << " resonator pings.\n";

  offline::OfflineRenderer renderer({kSampleRate, plan.framesHint});
  renderer.mixSamplerEvents(plan.samplerEvents);
  renderer.mixResonatorEvents(plan.resonatorEvents);

  if (options.exportWav) {
    const auto &pcm = renderer.finalize();
    const std::string path = "out/intro-sprout.wav";
    if (offline::OfflineRenderer::exportWav(path,
                                           static_cast<std::uint32_t>(kSampleRate),
                                           pcm)) {
      std::cout << "[sprout] bounced the mix into /out/intro-sprout.wav.\n";
    } else {
      std::cerr << "[sprout] failed to write /out/intro-sprout.wav" << std::endl;
      return 1;
    }
  } else {
    std::cout << "[sprout] pass --export-wav to bounce the quiet take into /out/.\n";
  }
  return 0;
}

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "Seed.h"
#include "engine/Engine.h"

#include "../shared/offline_renderer.hpp"

class HeadlessLoop {
 public:
  void onTick(std::function<void(int)> cb) { callback_ = std::move(cb); }

  void run(int cycles, int bpm) const {
    const bool quietMode = true;
    auto tickLength = std::chrono::milliseconds(60000 / (bpm * 4));
    std::cout << "[headless] quiet-mode=" << quietMode << ", bpm=" << bpm << "\n";

    for (int i = 0; i < cycles; ++i) {
      std::this_thread::sleep_for(quietMode ? tickLength / 8 : tickLength);
      if (callback_) {
        callback_(i);
      }
    }
    std::cout << "[headless] loop exited without spinning up audio IO.\n";
  }

 private:
  std::function<void(int)> callback_;
};

class GhostAutomation {
 public:
  void addLane(std::string name, int mod) {
    lanes_.push_back({std::move(name), mod, {}});
  }

  void tick(int frame) {
    for (auto &lane : lanes_) {
      const int period = std::max(1, lane.period);
      const float progress = static_cast<float>(frame % period) / static_cast<float>(period);
      const float value = 0.5f - 0.5f * std::cos(static_cast<float>(progress * 3.14159265f * 2.0f));
      lane.values.push_back(value);
      if (frame % period == 0) {
        std::cout << "  lane " << lane.name << " -> silent poke (frame " << frame << ", value="
                  << std::fixed << std::setprecision(2) << value << ")\n";
      }
    }
  }

  struct RenderPlan {
    std::vector<offline::ResonatorEvent> events;
    std::size_t framesHint = 0;
  };

  RenderPlan bakePlan(double sampleRate, double framesPerTick) const {
    RenderPlan plan;
    double maxWhen = 0.0;
    std::uint32_t idCounter = 100;

    for (std::size_t laneIndex = 0; laneIndex < lanes_.size(); ++laneIndex) {
      const auto &lane = lanes_[laneIndex];
      for (std::size_t i = 0; i < lane.values.size(); ++i) {
        const double when = framesPerTick * static_cast<double>(i);
        const auto whenSamples = static_cast<std::uint32_t>(std::lround(when));
        Seed seed{};
        seed.id = idCounter++;
        seed.engine = static_cast<std::uint8_t>(Engine::Type::kResonator);
        seed.pitch = static_cast<float>((laneIndex * 7.0) - 5.0 + lane.values[i] * 12.0f);
        seed.resonator.exciteMs = 4.0f + lane.values[i] * 5.0f;
        seed.resonator.damping = 0.28f + lane.values[i] * 0.55f;
        seed.resonator.brightness = 0.35f + lane.values[i] * 0.6f;
        seed.resonator.feedback = 0.58f + lane.values[i] * 0.32f;
        seed.resonator.mode = static_cast<std::uint8_t>((laneIndex + i) % 2);
        seed.resonator.bank = static_cast<std::uint8_t>((laneIndex + 1) % 5);
        plan.events.push_back({seed, whenSamples});
        maxWhen = std::max(maxWhen, static_cast<double>(whenSamples));
      }
    }

    const double tail = sampleRate * 3.0;
    plan.framesHint = static_cast<std::size_t>(std::lround(maxWhen + tail + sampleRate));
    return plan;
  }

  std::string toJson(double sampleRate, double secondsPerTick) const {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(4);
    oss << "{\n";
    oss << "  \"sample_rate\": " << sampleRate << ",\n";
    oss << "  \"tick_seconds\": " << secondsPerTick << ",\n";
    oss << "  \"lanes\": [\n";
    for (std::size_t i = 0; i < lanes_.size(); ++i) {
      const auto &lane = lanes_[i];
      oss << "    {\"name\": \"" << lane.name << "\", \"period\": " << lane.period
          << ", \"values\": [";
      for (std::size_t j = 0; j < lane.values.size(); ++j) {
        if (j != 0) {
          oss << ", ";
        }
        oss << lane.values[j];
      }
      oss << "]}";
      if (i + 1 != lanes_.size()) {
        oss << ",";
      }
      oss << "\n";
    }
    oss << "  ]\n";
    oss << "}\n";
    return oss.str();
  }

 private:
  struct Lane {
    std::string name;
    int period;
    std::vector<float> values;
  };

  std::vector<Lane> lanes_;
};

struct HeadlessOptions {
  bool exportArtifacts = false;
  bool showHelp = false;
};

HeadlessOptions parseArgs(int argc, char **argv) {
  HeadlessOptions opts;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--export") {
      opts.exportArtifacts = true;
    } else if (arg == "--help" || arg == "-h") {
      std::cout << "headless controls:\n"
                << "  --export  bounce /out/headless-automation.(wav|json)\n"
                << std::endl;
      opts.showHelp = true;
      return opts;
    }
  }
  return opts;
}

int main(int argc, char **argv) {
  constexpr double kSampleRate = 48000.0;
  const auto options = parseArgs(argc, argv);
  if (options.showHelp) {
    return 0;
  }
  HeadlessLoop loop;
  GhostAutomation automation;
  automation.addLane("filter", 6);
  automation.addLane("delay", 9);
  automation.addLane("vca", 4);

  loop.onTick([&](int frame) { automation.tick(frame); });
  const int kCycles = 24;
  const int kBpm = 72;
  loop.run(/*cycles=*/kCycles, /*bpm=*/kBpm);

  const double secondsPerTick = 60.0 / (static_cast<double>(kBpm) * 4.0);
  const double framesPerTick = kSampleRate * secondsPerTick;
  const auto plan = automation.bakePlan(kSampleRate, framesPerTick);
  std::cout << "[headless] staged " << plan.events.size() << " automation pings.\n";

  offline::OfflineRenderer renderer({kSampleRate, plan.framesHint});
  renderer.mixResonatorEvents(plan.events);

  if (options.exportArtifacts) {
    const auto &pcm = renderer.finalize();
    const std::string wavPath = "out/headless-automation.wav";
    const std::string jsonPath = "out/headless-automation.json";
    const std::string json = automation.toJson(kSampleRate, secondsPerTick);
    const bool wavOk = offline::OfflineRenderer::exportWav(
        wavPath, static_cast<std::uint32_t>(kSampleRate), pcm);
    const bool jsonOk = offline::OfflineRenderer::exportJson(jsonPath, json);
    if (wavOk && jsonOk) {
      std::cout << "[headless] bounced automation into /out/headless-automation.(wav|json).\n";
    } else {
      std::cerr << "[headless] failed to export automation artifacts." << std::endl;
      return 1;
    }
  } else {
    std::cout << "[headless] pass --export to bounce automation WAV + JSON into /out/.\n";
  }
  return 0;
}

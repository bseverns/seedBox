#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "Seed.h"
#include "engine/Engine.h"

#include "../shared/offline_renderer.hpp"
#include "../shared/reseed_playbook.hpp"

namespace {

struct BounceConfig {
  std::string tag;
  std::uint32_t seed;
  std::string wavPath;
};

constexpr double kSampleRate = 48000.0;
constexpr int kBpm = 124;
constexpr int kPasses = 3;

void auditionGarden(const std::vector<reseed::StemDefinition>& stems,
                    std::uint32_t seed,
                    int beatsPerPass) {
  std::mt19937 rng(seed);
  std::vector<reseed::StemDefinition> order = stems;
  const auto beatSpan = std::chrono::milliseconds(60000 / kBpm);
  const bool quietMode = true;
  std::cout << "[reseed] auditioning stems with seed=0x" << std::hex << seed << std::dec << "\n";
  for (int pass = 0; pass < kPasses; ++pass) {
    std::shuffle(order.begin(), order.end(), rng);
    for (int beat = 0; beat < beatsPerPass; ++beat) {
      const auto& stem = order[static_cast<std::size_t>(beat % order.size())];
      std::this_thread::sleep_for(quietMode ? beatSpan / 12 : beatSpan);
      std::cout << "  lane " << stem.lane << ": " << stem.name << " (ghost trigger)\n";
    }
  }
  std::cout << "[reseed] zero audio buffers touched during audition.\n";
}

bool bounceDeterministicStem(const BounceConfig& config,
                             const reseed::BouncePlan& plan,
                             std::vector<reseed::BounceLogBlock>& logs) {
  offline::OfflineRenderer renderer({kSampleRate, plan.framesHint});
  renderer.mixSamplerEvents(plan.samplerEvents);
  renderer.mixResonatorEvents(plan.resonatorEvents);
  const auto& pcm = renderer.finalize();
  if (!offline::OfflineRenderer::exportWav(config.wavPath,
                                           static_cast<std::uint32_t>(kSampleRate),
                                           pcm)) {
    std::cerr << "[reseed] failed to write " << config.wavPath << "\n";
    return false;
  }
  std::cout << "[reseed] bounced seed 0x" << std::hex << config.seed << std::dec << " -> "
            << config.wavPath << " (" << pcm.size() << " samples).\n";

  reseed::BounceLogBlock block;
  block.tag = config.tag;
  block.seed = config.seed;
  block.wavPath = config.wavPath;
  block.events = plan.logEntries;
  logs.push_back(block);
  return true;
}

void writeEventLog(const std::vector<reseed::StemDefinition>& stems,
                   const std::vector<reseed::BounceLogBlock>& logs,
                   const std::string& path) {
  const std::string json = reseed::serializeEventLog(stems, logs, kSampleRate, kBpm, kPasses);
  if (offline::OfflineRenderer::exportJson(path, json)) {
    std::cout << "[reseed] event log captured -> " << path << "\n";
  } else {
    std::cerr << "[reseed] failed to persist event log at " << path << "\n";
  }
}

}  // namespace

int main() {
  const auto& stems = reseed::defaultStems();
  auditionGarden(stems, 0xCAFEu, static_cast<int>(stems.size()));
  auditionGarden(stems, 0xBEEFu, static_cast<int>(stems.size()));

  std::vector<reseed::BounceLogBlock> logs;
  const reseed::BouncePlan planA = reseed::makeBouncePlan(stems, 0xCAFEu, kSampleRate, kBpm, kPasses);
  const reseed::BouncePlan planB = reseed::makeBouncePlan(stems, 0xBEEFu, kSampleRate, kBpm, kPasses);

  const bool okA = bounceDeterministicStem({"A", 0xCAFEu, "out/reseed-A.wav"}, planA, logs);
  const bool okB = bounceDeterministicStem({"B", 0xBEEFu, "out/reseed-B.wav"}, planB, logs);

  writeEventLog(stems, logs, "out/reseed-log.json");

  if (!okA || !okB) {
    std::cerr << "[reseed] offline render failed." << std::endl;
    return 1;
  }

  std::cout << "[reseed] reseed playback complete." << std::endl;
  return 0;
}


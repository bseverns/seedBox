#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "Seed.h"
#include "engine/Engine.h"

#include "offline_renderer.hpp"

namespace reseed {

enum class EngineKind { kSampler, kResonator };

struct StemDefinition {
  std::string name;
  int lane = 0;
  EngineKind engine = EngineKind::kSampler;
};

struct BounceLogEntry {
  std::string name;
  int lane = 0;
  std::uint32_t whenSamples = 0;
  std::uint32_t seedId = 0;
  std::uint32_t prng = 0;
  EngineKind engine = EngineKind::kSampler;
};

struct BouncePlan {
  std::vector<offline::SamplerEvent> samplerEvents;
  std::vector<offline::ResonatorEvent> resonatorEvents;
  std::vector<BounceLogEntry> logEntries;
  std::size_t framesHint = 0;
};

struct BounceLogBlock {
  std::string tag;
  std::uint32_t seed = 0;
  std::string wavPath;
  std::vector<BounceLogEntry> events;
};

inline EngineKind kindFromEngine(Engine::Type type) {
  return (type == Engine::Type::kResonator) ? EngineKind::kResonator : EngineKind::kSampler;
}

inline const std::vector<StemDefinition>& defaultStems() {
  static const std::vector<StemDefinition> stems = {
      {"kick compost", 0, EngineKind::kSampler},
      {"snare clipping", 1, EngineKind::kSampler},
      {"ride oxidation", 2, EngineKind::kResonator},
      {"hat patina", 3, EngineKind::kSampler},
  };
  return stems;
}

inline std::uint32_t mixSeed(std::uint32_t base, std::uint32_t salt) {
  constexpr std::uint32_t kPrime = 0x9E3779B1u;
  std::uint32_t v = base ^ (salt + kPrime);
  v ^= (v >> 16);
  v *= 0x7feb352du;
  v ^= (v >> 15);
  v *= 0x846ca68bu;
  v ^= (v >> 16);
  return v;
}

inline Seed makeSamplerSeed(const StemDefinition& stem,
                            std::uint32_t id,
                            std::uint32_t masterSeed,
                            int passIndex) {
  Seed seed{};
  seed.id = id;
  seed.lineage = masterSeed;
  seed.prng = mixSeed(masterSeed, id + static_cast<std::uint32_t>(passIndex * 7));
  seed.engine = static_cast<std::uint8_t>(Engine::Type::kSampler);
  seed.sampleIdx = static_cast<std::uint8_t>((stem.lane + passIndex) % 4);
  seed.pitch = -4.0f + static_cast<float>(stem.lane % 5);
  seed.envA = 0.0035f + 0.0008f * static_cast<float>(stem.lane + passIndex);
  seed.envD = 0.11f + 0.01f * static_cast<float>(passIndex % 3);
  seed.envS = 0.48f + 0.06f * static_cast<float>((stem.lane + 1) % 3);
  seed.envR = 0.17f + 0.015f * static_cast<float>(passIndex + stem.lane);
  seed.tone = 0.35f + 0.08f * static_cast<float>(stem.lane % 2);
  seed.spread = 0.18f + 0.16f * static_cast<float>((stem.lane + passIndex) % 3);
  seed.mutateAmt = 0.04f + 0.01f * static_cast<float>(stem.lane + 1);

  const std::string lower = stem.name;
  if (lower.find("kick") != std::string::npos) {
    seed.pitch = -9.0f + 0.5f * static_cast<float>(passIndex);
    seed.tone = 0.25f;
    seed.spread = 0.1f;
  } else if (lower.find("snare") != std::string::npos) {
    seed.pitch = -1.5f + 0.25f * static_cast<float>(passIndex);
    seed.tone = 0.6f;
    seed.envA = 0.0028f;
    seed.envD = 0.13f;
  } else if (lower.find("hat") != std::string::npos) {
    seed.pitch = 9.0f;
    seed.tone = 0.7f;
    seed.spread = 0.65f;
    seed.envD = 0.08f;
    seed.envR = 0.12f;
  }
  return seed;
}

inline Seed makeResonatorSeed(const StemDefinition& stem,
                              std::uint32_t id,
                              std::uint32_t masterSeed,
                              int passIndex) {
  Seed seed{};
  seed.id = id;
  seed.lineage = masterSeed;
  seed.prng = mixSeed(masterSeed ^ 0xBEEFCAFEu, id + static_cast<std::uint32_t>(passIndex * 11));
  seed.engine = static_cast<std::uint8_t>(Engine::Type::kResonator);
  seed.pitch = (stem.name.find("ride") != std::string::npos) ? 7.0f : -3.0f;
  seed.resonator.exciteMs = 4.2f + 0.35f * static_cast<float>((passIndex + stem.lane) % 3);
  seed.resonator.damping = 0.44f + 0.03f * static_cast<float>((stem.lane + passIndex) % 2);
  seed.resonator.brightness = 0.6f + 0.05f * static_cast<float>(passIndex % 3);
  seed.resonator.feedback = 0.58f + 0.04f * static_cast<float>((stem.lane % 2) + passIndex);
  seed.resonator.mode = 1;
  seed.resonator.bank = static_cast<std::uint8_t>(2 + (stem.lane % 3));
  return seed;
}

inline Seed makeSeed(const StemDefinition& stem,
                     std::uint32_t id,
                     std::uint32_t masterSeed,
                     int passIndex) {
  if (stem.engine == EngineKind::kResonator) {
    return makeResonatorSeed(stem, id, masterSeed, passIndex);
  }
  return makeSamplerSeed(stem, id, masterSeed, passIndex);
}

inline BouncePlan makeBouncePlan(const std::vector<StemDefinition>& stems,
                                 std::uint32_t masterSeed,
                                 double sampleRate,
                                 int bpm,
                                 int passes) {
  BouncePlan plan;
  if (stems.empty() || passes <= 0 || sampleRate <= 0.0 || bpm <= 0) {
    return plan;
  }

  const double framesPerBeat = sampleRate * (60.0 / static_cast<double>(bpm));
  std::mt19937 rng(masterSeed);
  std::vector<StemDefinition> order = stems;
  std::uint32_t seedCounter = 1;
  double maxWhen = 0.0;

  for (int pass = 0; pass < passes; ++pass) {
    std::shuffle(order.begin(), order.end(), rng);
    for (std::size_t idx = 0; idx < order.size(); ++idx) {
      const auto& stem = order[idx];
      const double beatIndex = static_cast<double>(pass * order.size() + idx);
      const double when = framesPerBeat * beatIndex;
      const auto whenSamples = static_cast<std::uint32_t>(std::lround(when));
      const Seed seed = makeSeed(stem, seedCounter++, masterSeed, pass);
      if (stem.engine == EngineKind::kResonator) {
        plan.resonatorEvents.push_back({seed, whenSamples});
      } else {
        plan.samplerEvents.push_back({seed, whenSamples});
      }
      plan.logEntries.push_back({stem.name, stem.lane, whenSamples, seed.id, seed.prng, stem.engine});
      maxWhen = std::max(maxWhen, static_cast<double>(whenSamples));
    }
  }

  const double tail = sampleRate * 2.75;
  plan.framesHint = static_cast<std::size_t>(std::lround(maxWhen + tail));
  return plan;
}

inline std::string engineToString(EngineKind engine) {
  return (engine == EngineKind::kResonator) ? "resonator" : "sampler";
}

inline std::string serializeEventLog(const std::vector<StemDefinition>& stems,
                                     const std::vector<BounceLogBlock>& bounces,
                                     double sampleRate,
                                     int bpm,
                                     int passes) {
  std::ostringstream oss;
  oss << "{\n";
  oss << "  \"sample_rate_hz\": "
      << static_cast<std::uint32_t>(std::lround(sampleRate)) << ",\n";
  oss << "  \"bpm\": " << bpm << ",\n";
  oss << "  \"passes\": " << passes << ",\n";
  oss << "  \"stems\": [\n";
  for (std::size_t i = 0; i < stems.size(); ++i) {
    const auto& stem = stems[i];
    oss << "    {\"lane\": " << stem.lane << ", \"name\": \"" << stem.name
        << "\", \"engine\": \"" << engineToString(stem.engine) << "\"}";
    if (i + 1 < stems.size()) {
      oss << ",";
    }
    oss << "\n";
  }
  oss << "  ],\n";
  oss << "  \"bounces\": [\n";
  for (std::size_t i = 0; i < bounces.size(); ++i) {
    const auto& bounce = bounces[i];
    oss << "    {\n";
    oss << "      \"tag\": \"" << bounce.tag << "\",\n";
    oss << "      \"seed\": " << bounce.seed << ",\n";
    if (!bounce.wavPath.empty()) {
      oss << "      \"wav\": \"" << bounce.wavPath << "\",\n";
    }
    oss << "      \"events\": [\n";
    for (std::size_t j = 0; j < bounce.events.size(); ++j) {
      const auto& evt = bounce.events[j];
      oss << "        {\"step\": " << j << ", \"when_samples\": " << evt.whenSamples
          << ", \"lane\": " << evt.lane << ", \"engine\": \""
          << engineToString(evt.engine) << "\", \"seed_id\": " << evt.seedId
          << ", \"prng\": " << evt.prng << ", \"name\": \"" << evt.name << "\"}";
      if (j + 1 < bounce.events.size()) {
        oss << ",";
      }
      oss << "\n";
    }
    oss << "      ]\n";
    oss << "    }";
    if (i + 1 < bounces.size()) {
      oss << ",";
    }
    oss << "\n";
  }
  oss << "  ]\n";
  oss << "}\n";
  return oss.str();
}

}  // namespace reseed


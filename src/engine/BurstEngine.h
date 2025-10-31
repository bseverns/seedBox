#pragma once

#include <cstdint>
#include <vector>

#include "engine/Engine.h"

//
// BurstEngine
// -----------
// Turns a single trigger into a deterministic burst of clustered events. The
// intent: demonstrate how scheduling math can be kept transparent without
// sacrificing musicality. State serialization mirrors EuclidEngine so reseed
// flows and lockouts stay deterministic.
class BurstEngine : public Engine {
public:
  enum class Param : std::uint16_t {
    kClusterCount = 0,
    kSpacingSamples = 1,
  };

  Engine::Type type() const noexcept override;
  void prepare(const Engine::PrepareContext& ctx) override;
  void onTick(const Engine::TickContext& ctx) override;
  void onParam(const Engine::ParamChange& change) override;
  void onSeed(const Engine::SeedContext& ctx) override;
  void renderAudio(const Engine::RenderContext& ctx) override;
  Engine::StateBuffer serializeState() const override;
  void deserializeState(const Engine::StateBuffer& state) override;

  const std::vector<std::uint32_t>& pendingTriggers() const { return pending_; }
  std::uint32_t generationSeed() const { return generationSeed_; }

private:
  std::uint8_t clusterCount_{1};
  std::uint32_t spacingSamples_{0};
  std::vector<std::uint32_t> pending_{};
  std::uint32_t generationSeed_{0};
  std::uint32_t lastSeedId_{0};
};


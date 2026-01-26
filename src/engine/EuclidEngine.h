#pragma once

#include <cstdint>
#include <vector>

#include "engine/Engine.h"

//
// EuclidEngine
// ------------
// Deterministic Euclidean rhythm generator wired into the shared engine
// contract. The implementation keeps state intentionally transparent so lessons
// about "steps vs. fills vs. rotation" can be taught straight from the code.
// Every reseed pins the engine to a known `masterSeed`, and the serialized state
// captures enough bread crumbs for lock flows to round-trip cleanly.
class EuclidEngine : public Engine {
public:
  enum class Param : std::uint16_t {
    kSteps = 0,
    kFills = 1,
    kRotate = 2,
  };

  Engine::Type type() const noexcept override;
  void prepare(const Engine::PrepareContext& ctx) override;
  void onTick(const Engine::TickContext& ctx) override;
  void onParam(const Engine::ParamChange& change) override;
  void onSeed(const Engine::SeedContext& ctx) override;
  void renderAudio(const Engine::RenderContext& ctx) override;
  Engine::StateBuffer serializeState() const override;
  void deserializeState(const Engine::StateBuffer& state) override;
  void panic() override;

  bool lastGate() const { return lastGate_; }
  std::uint32_t generationSeed() const { return generationSeed_; }
  const std::vector<std::uint8_t>& mask() const { return mask_; }
  std::uint8_t steps() const { return steps_; }
  std::uint8_t fills() const { return fills_; }
  std::uint8_t rotate() const { return rotate_; }

private:
  void rebuildMask();

private:
  std::uint8_t steps_{16};
  std::uint8_t fills_{4};
  std::uint8_t rotate_{0};
  std::vector<std::uint8_t> mask_{};
  std::uint32_t generationSeed_{0};
  std::uint32_t cursor_{0};
  bool lastGate_{false};
  std::uint32_t lastSeedId_{0};
};

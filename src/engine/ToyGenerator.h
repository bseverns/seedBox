#pragma once

#include <array>
#include <cstdint>

#include "engine/Engine.h"

// ToyGenerator is a minimal example engine for teaching and quick hacks.
// It renders short sine pings per seed trigger and avoids heap use in audio.
class ToyGenerator : public Engine {
public:
  static constexpr std::size_t kMaxVoices = 8;

  Engine::Type type() const noexcept override;
  void prepare(const PrepareContext& ctx) override;
  void onTick(const TickContext& ctx) override;
  void onParam(const ParamChange& change) override;
  void onSeed(const SeedContext& ctx) override;
  void renderAudio(const RenderContext& ctx) override;
  StateBuffer serializeState() const override;
  void deserializeState(const StateBuffer& state) override;
  void panic() override;

private:
  struct Voice {
    bool active{false};
    std::uint64_t startSample{0};
    float frequency{220.0f};
    float phase{0.0f};
    float amplitude{0.0f};
    float decaySeconds{0.25f};
    float leftGain{0.7f};
    float rightGain{0.7f};
  };

  void trigger(const Seed& seed, std::uint32_t whenSamples);

  std::array<Voice, kMaxVoices> voices_{};
  std::uint32_t nextVoice_{0};
  float sampleRate_{48000.0f};
  std::uint64_t renderSample_{0};
};


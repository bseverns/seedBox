#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

struct Seed;

//
// Engine (base class)
// --------------------
// Audio and control engines inside SeedBox share the same life cycle: prepare
// them with the current runtime context, let them react to transport ticks,
// poke parameters, fan out seeds, and finally render audio blocks. The teaching
// goal is to make that contract explicit so new engines (Euclid, Burst, future
// DSP toys) can be dropped in without spelunking the router. We keep the
// interface deliberately tiny and data-oriented — every hook receives a plain
// struct so tests and notebooks can forge contexts without dragging in the
// entire firmware runtime.
class Engine {
public:
  struct PrepareContext {
    bool hardware{false};
    std::uint32_t sampleRate{0};
    std::uint32_t framesPerBlock{0};
    std::uint32_t masterSeed{0};
  };

  struct TickContext {
    std::uint64_t tick{0};
  };

  struct ParamChange {
    std::uint32_t seedId{0};
    std::uint16_t id{0};
    std::int32_t value{0};
  };

  struct SeedContext {
    const Seed& seed;
    std::uint32_t whenSamples{0};
  };

  struct RenderContext {
    float* left{nullptr};
    float* right{nullptr};
    std::size_t frames{0};
  };

  using StateBuffer = std::vector<std::uint8_t>;

  virtual ~Engine() = default;

  virtual void prepare(const PrepareContext& ctx) = 0;
  virtual void onTick(const TickContext& ctx) = 0;
  virtual void onParam(const ParamChange& change) = 0;
  virtual void onSeed(const SeedContext& ctx) = 0;
  virtual void renderAudio(const RenderContext& ctx) = 0;
  virtual StateBuffer serializeState() const = 0;
  virtual void deserializeState(const StateBuffer& state) = 0;
};


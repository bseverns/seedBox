# Add a generator (engine)

This guide shows the minimal interface and a working toy generator you can copy.

## Minimal engine template

```cpp
// MyEngine.h
#pragma once

#include "engine/Engine.h"

class MyEngine : public Engine {
public:
  Engine::Type type() const noexcept override { return Engine::Type::kUnknown; }
  void prepare(const PrepareContext& ctx) override { (void)ctx; }
  void onTick(const TickContext& ctx) override { (void)ctx; }
  void onParam(const ParamChange& change) override { (void)change; }
  void onSeed(const SeedContext& ctx) override { (void)ctx; }
  void renderAudio(const RenderContext& ctx) override { (void)ctx; }
  StateBuffer serializeState() const override { return {}; }
  void deserializeState(const StateBuffer& state) override { (void)state; }
};
```

## Example: ToyGenerator (compiled in sim)

The repo includes a tiny example engine that renders short sine pings without
allocating on the audio thread:

- `src/engine/ToyGenerator.h`
- `src/engine/ToyGenerator.cpp`

Key patterns it demonstrates:

- `onSeed()` stores a trigger with the provided `whenSamples` timestamp.
- `renderAudio()` mixes a fixed pool of voices into the output buffer.
- No heap allocation or logging inside `renderAudio()`.

## Wire it into the router

1. Add a new engine ID in `src/engine/EngineRouter.h`.
2. Register the engine in `src/engine/EngineRouter.cpp`:

```cpp
registerEngine(kToyId, "Toy", "TOY", std::make_unique<ToyGenerator>());
```

3. (Optional) Add a UI label if your engine needs a friendly name.

## Hear it in sim

- Build and run the native sim.
- Switch a seed's engine to `EngineRouter::kToyId` (ID 5 in this repo).
- The toy engine will render short sine bursts on each trigger.

## Safety checklist

- Keep state in fixed-size arrays.
- Use `whenSamples` for scheduling, not `std::this_thread::sleep`.
- Clamp amplitudes to avoid runaway output.
- Implement `panic()` to silence voices.

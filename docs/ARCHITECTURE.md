# Architecture guide

This is the "where to add a feature" map. The key mental model is a strict
control-rate loop feeding an audio-rate render path.

## Audio-rate vs control-rate boundaries

- **Audio rate**: `Engine::renderAudio()` runs inside the audio callback and
  must be real-time safe. See `src/app/AppState.cpp` (`handleAudio`) and
  `src/hal/hal_audio.cpp`.
- **Control rate**: `AppState::tick()` is the per-frame control loop. It handles
  UI input, mode changes, reseeds, and scheduling. See `src/app/AppState.cpp`.
- **Clocking**: `ClockProvider` and `InternalClock` live at control rate and
  push ticks into the scheduler. See `include/app/Clock.h` and `src/app/Clock.cpp`.

## Data flow diagram

```
ClockProvider
   |  (tick)
   v
PatternScheduler
   |  (seed + whenSamples)
   v
EngineRouter
   |  (Engine::onSeed)
   v
Engine render
   |  (audio buffer)
   v
Output (HAL audio)
```

## Safe patterns checklist

- No heap allocation in `renderAudio()` or any path it calls.
- No logging, file I/O, or locks on the audio thread.
- Keep per-voice state in fixed-size arrays or preallocated buffers.
- Clamp user input (density, probability, feedback) before it hits DSP.
- Use `whenSamples` for sample-accurate timing; do not spin-wait.
- Use `panic()` to reset voice state and clear queues.

## Where each layer lives

- **Clock + transport**: `include/app/Clock.h`, `src/app/Clock.cpp`
- **Control loop**: `src/app/AppState.cpp` (`tick`, `processInputEvents`)
- **Seed generation**: `src/app/AppState.cpp` (`primeSeeds`, `build*Seeds`)
- **Scheduler**: `src/engine/Patterns.h`, `src/engine/Patterns.cpp`
- **Router + engine registry**: `src/engine/EngineRouter.h`, `src/engine/EngineRouter.cpp`
- **Engines (generators)**:
  - Sampler: `src/engine/Sampler.h`, `src/engine/Sampler.cpp`
  - Granular: `src/engine/Granular.h`, `src/engine/Granular.cpp`
  - Resonator: `src/engine/Resonator.h`, `src/engine/Resonator.cpp`
  - Euclid: `src/engine/EuclidEngine.h`, `src/engine/EuclidEngine.cpp`
  - Burst: `src/engine/BurstEngine.h`, `src/engine/BurstEngine.cpp`
  - Toy example: `src/engine/ToyGenerator.h`, `src/engine/ToyGenerator.cpp`
- **Audio output + HAL**: `src/hal/hal_audio.h`, `src/hal/hal_audio.cpp`

## Quick orientation path

1. Start in `src/app/AppState.cpp` to see control flow (`tick` -> scheduler).
2. Read `src/engine/Patterns.cpp` to understand event timing.
3. Jump to `src/engine/EngineRouter.cpp` to see how seeds route to engines.
4. Inspect an engine (`Sampler.cpp`) to see how `onSeed` turns into audio.

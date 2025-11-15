# Engine Field Notes

> "The intent: **any seed rendered in rehearsal behaves the same on stage**." — `Sampler.h`

This notebook-y README is the cheat-sheet for the five engines that currently live under `src/engine/`. Each section sketches what the engine is for, which slices of the `Seed` genome it inhales, and where to hook your debugger (`prepare`, `onSeed`, `onTick`). Follow the roadmap links for broader context, and peep the matching tests whenever you want to see the guard rails in action.

## Sampler
*Live sample voices riding a deterministic voice pool.*

> "Every attribute comes from the Seed genome (`Seed::env*`, `Seed::tone`, etc.), so deterministic reseeding is as easy as re-running the scheduler." — [`Sampler.h`](Sampler.h)

- **Seed parameters**: `pitch`, `sampleIdx`, `envA/envD/envS/envR`, `tone`, `spread`, and the implicit SD/RAM switch determined by `sampleIdx`. These get baked into `VoiceInternal` inside [`configureVoice`](Sampler.cpp#L166-L180) right before the trigger launches.【F:src/engine/Sampler.cpp†L166-L180】
- **Entry points**:
  - `prepare` wires up the shared engine context and resets the voice handles.
  - `onSeed(const Engine::SeedContext&)` caches the incoming `Seed` so UI + tests can recall it later, then defers to `trigger` via the scheduler.【F:src/engine/Sampler.cpp†L232-L261】
  - `onTick` advances envelopes and handles scheduled launches during each audio tick.
- **Tests**: [`test_sampler_voice_pool.cpp`](../../tests/test_engine/test_sampler_voice_pool.cpp) stress the four-voice cap and assert that `spread`, envelopes, and playback rate match the serialized seed plan.
- **Roadmap**: [`seed_system.md`](../../docs/roadmaps/seed_system.md) keeps the sampler marching orders tied to the global seed workflow.

## Granular
*Grain clouds planned from the seed genome and mapped to either the sim or Teensy graph.*

> "Seeds map deterministically into `GrainVoice` plans that tests can snapshot, making it possible to teach the whole DSP chain straight from the code." — [`Granular.h`](Granular.h)

- **Seed parameters**: `pitch`, the entire `Seed::granular` block (`grainSizeMs`, `sprayMs`, `transpose`, `windowSkew`, `stereoSpread`, `source`, `sdSlot`), plus the per-seed RNG (`prng`). `planGrain` does the heavy lifting, deriving playback rate and routing before the voice hits the graph.【F:src/engine/Granular.cpp†L256-L297】
- **Entry points**:
  - `prepare` selects sim vs. hardware mode and resets the shared state.
  - `onSeed` mirrors the sampler: cache first, then schedule via `trigger` so deterministic reseeds look identical.【F:src/engine/Granular.cpp†L339-L365】
  - `onTick` is where grain launches are checked against the timeline each audio callback.
- **Tests**: [`test_granular_voice_budget.cpp`](../../tests/test_engine/test_granular_voice_budget.cpp) confirms the voice allocator honors the configured pool size and that grain plans preserve seed-sourced timing.
- **Roadmap**: [`granular.md`](../../docs/roadmaps/granular.md) walks through the staged build-out, including live input fan-out vs. SD clip playback.

## Resonator
*Karplus/modal pings that translate seed DNA into modal banks.*

> "Helper trio: choose a voice slot, translate the seed into modal parameters, and then map that plan onto either hardware nodes or simulator state." — [`Resonator.h`](Resonator.h)

- **Seed parameters**: `pitch`, `resonator.exciteMs`, `resonator.damping`, `resonator.brightness`, `resonator.feedback`, `resonator.mode`, `resonator.bank`, and the global `prng`. `planExcitation` clamps modes, resolves presets, then blends seed brightness/feedback with preset defaults before shipping the voice plan to hardware/sim land.【F:src/engine/Resonator.cpp†L216-L273】
- **Entry points**:
  - `prepare` seeds modal presets, resets handles, and readies the fan-out mixers.
  - `onSeed` again caches the genome before invoking `trigger` so voice planning stays inspectable from tests and UI.【F:src/engine/Resonator.cpp†L266-L379】
  - `onTick` would host per-callback housekeeping (currently minimal while the hardware graph solidifies).
- **Tests**: [`test_resonator_voice_pool.cpp`](../../tests/test_engine/test_resonator_voice_pool.cpp) inspects modal allocations, checking that `seedId`, damping, and preset pointers line up with the cached voice state.
- **Roadmap**: [`resonator.md`](../../docs/roadmaps/resonator.md) plots the migration from simulator scaffolding to fully wired Teensy resonance.

## Euclid
*Transparent Euclidean rhythm masks with seed-aware serialization.*

> "Deterministic Euclidean rhythm generator wired into the shared engine contract. The implementation keeps state intentionally transparent so lessons about `steps vs. fills vs. rotation` can be taught straight from the code." — [`EuclidEngine.h`](EuclidEngine.h)

- **Seed parameters**: Euclid mostly watches for `SeedContext` metadata—`whenSamples` for scheduling and `seed.id` for provenance—while rhythm math is driven through `Param` changes (`steps`, `fills`, `rotate`). The master seed from `prepare` (`ctx.masterSeed`) primes `generationSeed_` so reseeds remain repeatable.【F:src/engine/EuclidEngine.cpp†L29-L64】【F:src/engine/EuclidEngine.cpp†L66-L115】
- **Entry points**:
  - `prepare` resets the mask, cursor, and seeds it with the shared master seed.【F:src/engine/EuclidEngine.cpp†L54-L64】
  - `onSeed` records the latest `seed.id` and defers to the scheduler for when the next gate lands.【F:src/engine/EuclidEngine.cpp†L91-L96】
  - `onTick` advances the cursor, evaluating the Euclidean mask one sample block at a time.【F:src/engine/EuclidEngine.cpp†L66-L89】
- **Tests**: [`test_euclid_burst.cpp`](../../tests/test_engine/test_euclid_burst.cpp) covers both Euclid and Burst engines, asserting deterministic masks and serialization round-trips.
- **Roadmap**: [`euclid_burst.md`](../../docs/roadmaps/euclid_burst.md) maps out how Euclid patterns and burst clusters dovetail inside the scheduler.

## Burst
*Cluster scheduling physics distilled into two parameters.*

> "Turns a single trigger into a deterministic burst of clustered events." — [`BurstEngine.h`](BurstEngine.h)

- **Seed parameters**: Burst keeps it sparse—it primarily consumes the `SeedContext` (`seed.id`, `whenSamples`) while `clusterCount` and `spacingSamples` arrive via engine params. The engine does stash the master seed from `prepare` so serialization lines up with Euclid-style reseeds.【F:src/engine/BurstEngine.cpp†L34-L78】【F:src/engine/BurstEngine.cpp†L80-L110】
- **Entry points**:
  - `prepare` clears pending triggers and stores the session’s `masterSeed`.【F:src/engine/BurstEngine.cpp†L34-L46】
  - `onSeed` expands the single trigger into a burst timeline, populating `pending_` with deterministic offsets.【F:src/engine/BurstEngine.cpp†L80-L95】
  - `onTick` is intentionally lean—the scheduling math lives in `onSeed`, so ticks just coast unless future work needs runtime modulation.【F:src/engine/BurstEngine.cpp†L48-L52】
- **Tests**: Again see [`test_euclid_burst.cpp`](../../tests/test_engine/test_euclid_burst.cpp) for coverage of the burst queue semantics.
- **Roadmap**: [`euclid_burst.md`](../../docs/roadmaps/euclid_burst.md) details the shared sequencing plan with Euclid.

---

When in doubt, crack open the headers linked above—they’re intentionally annotated so the engines double as lecture notes. Then run the matching tests to watch those theories get exercised in code.

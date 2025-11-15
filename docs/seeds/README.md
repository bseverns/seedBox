# Seed Atlas & Routing Field Notes

Welcome to the zine for Seed genomes. The goal is to narrate how the firmware
teaches itself what to play: we annotate the DNA, follow the plumbing from
AppState into the Sampler engine, and record experiments along the way.

## Genome cheat sheet

| Field | Purpose | Where it lands |
| --- | --- | --- |
| `id` | Table index + debug anchor. AppState assigns it sequentially so tests can pluck a deterministic seed. | `PatternScheduler` indexes voices and EngineRouter chooses a lane with it. |
| `prng` | Captured RNG state from minting. Reuse this to re-roll deterministic per-seed randomness. | Engines like Euclid reuse the bitstream when mutating patterns. |
| `source`/`lineage` | Human-facing provenance: LFSR prime, tap tempo BPM tag, preset slot. | UI surfaces it, EngineRouter passes through untouched for analytics. |
| `pitch` | Semitone offset from concert A. | `Sampler::configureVoice` converts to playback rate with `pitchToPlaybackRate`. |
| `envA/D/S/R` | ADSR block in seconds. | Sampler maps to milliseconds for the Teensy envelope, Resonator shapes modal decay. |
| `density` & `probability` | Scheduler knobs for how often and whether a voice fires. | `PatternScheduler` queries both when queuing `Engine::SeedContext`. |
| `jitterMs` | Timing spray in milliseconds. | Scheduler offsets trigger timestamps by this wander. |
| `tone` | Tilt-EQ macro. | Sampler maps 0–1 to 400–8000 Hz, Resonator biases brightness. |
| `spread` | Stereo width macro. | `stereo::constantPowerWidth` sets per-voice mixer gains. |
| `engine` | Engine lane selector (0 sampler, 1 granular, 2 resonator). | `EngineRouter::triggerSeed` sanitizes it and calls the target engine. |
| `sampleIdx` | Wavetable slot request. | Sampler picks RAM vs SD streaming based on this index. |
| `mutateAmt` | Bound for random walk editors. | `SeedLock` and mutate UI gestures use it as the guard rail. |
| `granular.*` | Grain dimensions, source selection, and stereo spread. | `GranularEngine::configureVoice` (see src/engine/Granular.cpp) consumes these fields directly. |
| `resonator.*` | Excitation burst + modal bank tuning. | `ResonatorBank::configureVoice` reads these straight into the modal engine. |

These annotations live directly in [`include/Seed.h`](../../include/Seed.h) so the
struct tells its own story during code walkthroughs.【F:include/Seed.h†L1-L49】

## Routing tour: AppState → EngineRouter → Sampler

1. **Prime the seed table.** `AppState::primeSeeds` hydrates the four default
   entries from `buildLfsrSeeds`, tagging each seed with its table index and RNG
   state.【F:src/app/AppState.cpp†L926-L1002】【F:src/app/AppState.cpp†L1044-L1075】
2. **Schedule the pattern.** The same method rebuilds the `PatternScheduler`,
   installs `EngineRouter::dispatchThunk` as the trigger callback, and drops the
   freshly-minted seeds into the scheduler queue.【F:src/app/AppState.cpp†L998-L1022】
3. **Dispatch to engines.** When the scheduler fires it hands the Seed to
   `EngineRouter::triggerSeed`, which sanitizes the `engine` id and forwards the
   `Engine::SeedContext` to the chosen lane (sampler by default).【F:src/engine/EngineRouter.cpp†L24-L63】
4. **Sampler bakes the DNA.** `Sampler::trigger` allocates a voice, increments
   the deterministic handle, and funnels every seed parameter through
   `configureVoice`. Envelope values get clamped, stereo width becomes
   constant-power gains, and the pitch offset morphs into a playback rate via
   `pitchToPlaybackRate`.【F:src/engine/Sampler.cpp†L120-L205】

## Experiment log: sim-only pitch offset

I temporarily nudged `buildLfsrSeeds` to add a +7 semitone offset whenever we
compile for the native simulator. That ensures we could hear an obvious jump in
pitch when the scheduler rolled the sampler. The change flowed from the seed
prime straight into `Sampler::configureVoice`, where the playback rate scales by
`pow(2, pitch / 12)` — a +7 semitone boost is ≈1.498x playback speed, so RAM
voices should chirp brighter.【F:src/engine/Sampler.cpp†L175-L205】

Running `python -m platformio run -e native` rebuilds the sim; if your studio rig
is air-gapped or a CI sandbox, make sure you've already mirrored the PlatformIO
platforms you care about so the build can rage offline. After capturing the
pitch delta I dropped the hack so the repo stays clean.

## Tooling sketches

* **Seed diff visualizer.** Render before/after values for a seed mutation in a
  table, highlighting changes beyond a configurable tolerance.
* **Playback audition harness.** Wrap `Sampler::voice` state dumping in a script
  that prints effective playback rate, envelope milliseconds, and spread values
  so students can correlate ears with data without cracking a debugger.
* **Genome templating.** Provide YAML snippets for common archetypes (e.g.,
  staccato sampler hit, lush granular pad) that hydrate Seeds through
  `AppState::setSeedPreset`, giving the UI a curated starting deck.

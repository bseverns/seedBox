# Scratchpad — granular voices + reseed locks

Keeping the granular roadmap and the reseeding plan stitched together. Rough
edges welcome.

## Granular roadmap name-drops (for spelunking later)

* Files: `engine/Granular.*`, Teensy `AudioEffectGranular`, scheduler glue via
  `EngineRouter` and `PatternScheduler`.【F:docs/roadmaps/granular.md†L1-L56】
* Constants + members: `GranularEngine::kVoicePoolSize`,
  `GranularEngine::Source::{kLiveInput,kSdClip}`, `Seed::granular.sdSlot`,
  `Seed::granular.{grainSizeMs,sprayMs,windowSkew,stereoSpread,source}`,
  `Seed::pitch`, `Seed::granular.transpose`, `GrainVoice::dspHandle`, and
  `GranularEngine::GrainVoice` overall.【F:docs/roadmaps/granular.md†L10-L70】
* Functions/hotspots worth tracing: `registerSdClip`,
  `GranularEngine::trigger`, `EngineRouter::dispatchThunk`,
  `activeVoiceCount()`, `beginPitchShift()`, `setGrainLength()`, and the planner's
  reliance on `Seed::granular.source` when flipping mixers.【F:docs/roadmaps/granular.md†L20-L59】【F:docs/roadmaps/granular.md†L72-L79】

## Current coverage snapshots

* `test_granular_voice_budget.cpp` proves the voice pool caps correctly, steals
  the oldest grain, maps SD slots, and even stops a phantom SD player when the
  slot metadata is missing.【F:test/test_engine/test_granular_voice_budget.cpp†L24-L103】
* The reseed README draft (`test/test_app/README.md`) maps the seed-system doc to
  app-level tests so we can see which promises already have regression eyes on
  them.【F:test/test_app/README.md†L1-L41】

## Gaps + gut-checks

* Roadmap still sketches CPU sanity chores (perf HUD, histogram, SD stress,
  mixer profiling) with no automation yet. Flagging them here so they don't fall
  off the radar when we chase DSP polish.【F:docs/roadmaps/granular.md†L72-L79】
* No automated probe for the Teensy `AudioEffectGranular` quirks—tests only check
  the sim stub today. A smoke test targeting hardware builds would verify the
  `beginPitchShift()` fallback stays wired.【F:docs/roadmaps/granular.md†L40-L46】【F:test/test_engine/test_granular_voice_budget.cpp†L86-L103】
* `pio test -e native -f test_engine` can't currently fetch the `native` platform
  because the registry blocks our outbound requests (403), so the coverage story
  still lives on paper until CI (or a cached toolchain) comes online.【eddf39†L1-L9】

## Next deep-dive targets

* Extend the granular suite so it inspects `GrainVoice::dspHandle` mappings and
  mixer fan-out, mirroring the roadmap's graph sketch.【F:docs/roadmaps/granular.md†L61-L70】
* Add tap-tempo + preset prime mode tests alongside the live-input checks to make
  the reseeding roadmap symmetrical.【F:test/test_app/README.md†L23-L41】

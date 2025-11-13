# Seed reseed notebook (draft)

We use this folder to make sure the reseeding roadmap in [`docs/roadmaps/seed_system.md`](../../docs/roadmaps/seed_system.md)
stays honest. Treat it like a scribbled margin note: half lab report, half punk-rock
lesson plan.

## Roadmap promises we're shadowing

* `AppState::primeSeeds` is the main conductor, fed by four prime modes:
  `SeedPrimeMode::kLfsr`, `SeedPrimeMode::kTapTempo`, `SeedPrimeMode::kPreset`,
  and `SeedPrimeMode::kLiveInput`. Each mode stamps `Seed::source` and
  `Seed::lineage`, while the live path also sets
  `GranularEngine::Source::kLiveInput` + `Seed::granular.sdSlot = 0`.【F:docs/roadmaps/seed_system.md†L6-L24】
* Locks ride through `SeedLock`, exposing `seedPageToggleLock`,
  `seedPageToggleGlobalLock`, `isSeedLocked`, and `isGlobalSeedLocked` to the UI.【F:docs/roadmaps/seed_system.md†L26-L43】
* `seedPageNudge` and `applyQuantizeControl` lean on `util::ScaleQuantizer`
  helpers so pitch edits land on-grid without muting creativity.【F:docs/roadmaps/seed_system.md†L45-L57】
* Every genome change pings `EngineRouter::onSeed` so the sampler, granular
  engine, and resonator cache the same truth the UI shows.【F:docs/roadmaps/seed_system.md†L59-L70】

## Tests already covering that story

* `test_seed_lock_behaviour.cpp` proves per-seed locks survive reseeds and engine
  swaps, and that the global lock freezes the table exactly as advertised. It
  also pokes the quantize control via the MN42 CC map to ensure the sampler hears
  the snapped pitch immediately.【F:tests/test_app/test_seed_lock_behaviour.cpp†L11-L74】
* `test_seed_prime_modes.cpp` focuses on the live-input prime mode, asserting we
  tag every seed with `Seed::Source::kLiveInput` plus the granular live slot, and
  that engine assignments stay untouched across a reseed.【F:tests/test_app/test_seed_prime_modes.cpp†L9-L41】
* `test_presets.cpp` stress-tests the preset lane promised in the roadmap by
  round-tripping a captured genome set through the EEPROM store and verifying the
  crossfade helper preserves granular params during the blend.【F:tests/test_app/test_presets.cpp†L13-L48】
* `test_granular_source_toggle.cpp` makes sure the shift+tone UI helpers can flip
  a granular seed between live input and SD slots while keeping the scheduled
  seed in sync with what the display prints.【F:tests/test_app/test_granular_source_toggle.cpp†L1-L112】

## Open questions + follow-ups

* The roadmap calls out tap-tempo (`recordTapTempoInterval`) and preset primes,
  but we only have an automated check for the live-input flow. We should either
  add tap-tempo + preset prime tests or explicitly document why they ride on the
  legacy LFSR path without bespoke coverage.【F:docs/roadmaps/seed_system.md†L10-L24】【F:tests/test_app/test_seed_prime_modes.cpp†L9-L41】
* `util::ScaleQuantizer` has `SnapUp` and `SnapDown` hooks mentioned in the doc,
  but the test suite only hits the default snap-to-scale path. Worth scoping a
  regression that nudges a seed upward/downward to prove we honor those future UI
  affordances.【F:docs/roadmaps/seed_system.md†L51-L57】【F:tests/test_app/test_seed_lock_behaviour.cpp†L55-L74】
* Engine hand-offs are claimed to be deterministic through `EngineRouter::onSeed`,
  yet we currently rely on indirect evidence (sampler cache reads). A targeted
  check against the granular/resonator caches would make the roadmap's promise
  explicit in code.【F:docs/roadmaps/seed_system.md†L59-L70】【F:tests/test_app/test_seed_lock_behaviour.cpp†L31-L43】

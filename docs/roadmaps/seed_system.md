# Seed system refresh — multi-source genomes + locks

This page is the field guide for the seed overhaul that just landed in
`AppState`. Treat it as a studio notebook entry: part intention, part wiring
diagram for anyone teaching or hacking on the sequencer layer.

## Seed sources (aka where the genomes come from)

We now treat "seed" as a thing with lineage:

| Mode | How to select | What it does | Determinism hook |
|------|---------------|--------------|------------------|
| **LFSR** | default, `SeedPrimeMode::kLfsr` | Xorshift-driven, matches the old behaviour byte-for-byte. | `Seed::source = Seed::Source::kLfsr`, `Seed::lineage = masterSeed`. |
| **Tap tempo** | call `setSeedPrimeMode(SeedPrimeMode::kTapTempo)` and feed `recordTapTempoInterval(ms)` | Rerolls the table while respecting the player's tempo. Density tightens to the average tap, jitter is halved so grooves stay punchy. | `Seed::source = Seed::Source::kTapTempo`, `Seed::lineage = bpm * 100` (rounded). |
| **Preset** | load a bank via `setSeedPreset(id, seeds)` then call `seedPageReseed(..., SeedPrimeMode::kPreset)` | Copies curated genomes (think classroom examples) but still keeps the engine/scheduler glue live. | `Seed::source = Seed::Source::kPreset`, `Seed::lineage = preset id`. |
| **Live input** | rotate the front-panel prime mode until "Live" shows up, or call `seedPageReseed(..., SeedPrimeMode::kLiveInput)` | Spins the same RNG genomes as LFSR but tags the lineage so *whatever* engine you park on that seed can ride the realtime input. When you swing over to the granular engine it latches the SGTL5000 I²S feed; sim builds keep the tap muted. SD clip bookkeeping gets wiped so the slot is primed for sampling mid-jam. | `Seed::source = Seed::Source::kLiveInput`, `Seed::lineage = masterSeed`, `Seed::granular.source = GranularEngine::Source::kLiveInput`, `Seed::granular.sdSlot = 0`. |

All four paths flow through `AppState::primeSeeds`, so reseeding from the Seed
page or a MIDI hook stays deterministic. Locked seeds keep their previous genome
no matter which source we pivot to.

## Lock choreography

`SeedLock` is the tiny manager that keeps per-seed and global locks in sync.
Everything funnels through the Seed page helpers:

- `seedPageToggleLock(index)` — short-press the lock button, or call directly
  from the UI. The indexed seed keeps its genome and engine assignment on the
  next reseed.
- `seedPageToggleGlobalLock()` — long-press the lock button. The whole table is
  frozen; reseeds only bump the master seed counter so notebooks stay honest.
- `isSeedLocked(index)` / `isGlobalSeedLocked()` — sanity probes for UI chrome
  and tests.

Button gestures:

- **Reseed** button (pin 2): short press spins the master seed (same as before).
- **Lock** button (pin 3):
  - Short press → per-seed lock/unlock targeting the focused seed.
  - Long press (~600 ms) → global lock toggle.

The `SeedLock` manager lives outside the scheduler so PatternScheduler stays
oblivious — deterministic triggers first, pedagogy second.

## Nudge + quantize controls

The Seed page can now request deliberate genome edits:

- `seedPageNudge(index, AppState::SeedNudge{...})` applies bounded deltas to
  pitch/density/probability/etc. Locked seeds ignore the request.
- Quantize control: MN42 CC 18 encodes scale + root (scale index lives in the
  upper bits, root note in the lower nibble). `applyQuantizeControl` snaps the
  focused seed to the requested scale using the new `util::ScaleQuantizer`
  helpers, then re-broadcasts the genome to whichever engine owns that seed.

`ScaleQuantizer` exposes `SnapToScale`, `SnapUp`, and `SnapDown` for future UI
experiments (e.g. "always resolve up" knobs).

## Engine-facing guarantees

Every time a seed changes shape we call `EngineRouter::onSeed`, which fans the
update out to the sampler, granular engine, or resonator depending on the
current assignment. Each engine caches the last genome per seed ID so tests and
teaching tools can diff "what the scheduler thinks" vs. "what the DSP is
holding" without firing a trigger.

That makes these flows deterministic:

1. Reseed (any source) → seeds rebuilt/merged → `onSeed` fired per slot.
2. Engine swap (`setSeedEngine`) → scheduler updated → owning engine receives a
   fresh genome copy immediately.
3. Quantize/nudge → scheduler updated → engines stay in lockstep with the UI.

The TL;DR for students: lock what you love, reseed the rest, and the engines
will always hear exactly what the UI displays.

## Regression safety net

Unity tests keep us honest:

- `test_seed_lock_behaviour.cpp` asserts that per-seed locks and the global lock
  survive reseeds and even engine swaps without desyncing the router caches.
- The same suite also smokes the Quantize control by faking the MN42 CC stream
  and making sure the sampler hears the snapped pitch immediately.
- `test_seed_prime_modes.cpp` now includes a simulator walk that fires a
  granular voice on the live-input lane, proving the "live-in" alias is respected
  even with the codec stubbed out.【F:tests/test_app/test_seed_prime_modes.cpp†L55-L90】

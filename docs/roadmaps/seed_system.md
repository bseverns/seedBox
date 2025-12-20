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

## Live-input gate (tempo-locked reseeds)

The seed table now listens for live-input energy and rewrites the focused slot
on a quantized gate. Three moving parts, all deliberately tiny so you can walk a
class through the math:

1. **State lives in `AppState`.** We track `inputGateLevel_`, `inputGatePeak_`,
   `inputGateHot_`, and `lastGateTick_`, plus a `gateDivision_` enum covering
   `1/1`, `1/2`, `1/4`, and full bars. Host code can poke the values via
   `setInputGateDivisionFromHost(...)` or tweak the RMS/peak floor with
   `setInputGateFloorFromHost(...)`; the Seed page UI exposes the same thing via
   Shift + Density (hold and twist) so hardware and JUCE stay in sync.
2. **Energy probe rides the dry input path.** `setDryInputFromHost` (JUCE) and
   the hardware audio callback both compute per-block RMS/peak and mark the gate
   "hot" if either crosses the floor (`inputGateFloor_` defaults to the engine
   passthrough floor so denorm fuzz never trips it). Hot → cold transitions
   don't immediately clear the edge; we keep a `gateEdgePending_` flag until a
   tempo tick consumes it.
3. **Quantize to the transport, then reseed.** `handleGateTick()` runs once per
   scheduler tick (24 PPQN). When a gate edge is pending and the chosen division
   interval has elapsed (`gateDivisionTicks()` fans out to 1 beat, half-beats,
   sixteenths, or 4-beat bars), we call
   `seedPageReseed(RNG::xorshift(masterSeed_), seedPrimeMode_)` to rewrite the
   table. Locked slots or a global lock short-circuit the request so notebooks
   stay reproducible.

Players get live feedback: the metrics line now shows `G` + division + a gate
glyph (`^` when queued, `!` when hot, `-` when idle) and the Seed page hint
calls out the current division. It's deliberately lo-fi — think stage notes,
not a DAW meter.

## Momentary live-capture button (short sample bank jams)

Think of this as a two-second Polaroid shutter: slap a button, bottle the live
input into a tiny bank, and deterministically pick which snapshot drives the
next reseed. Wiring it up is all plumbing and PRNG hygiene.

1. **Pick a spare GPIO and debounced edge.** Mirror the reseed/lock handling in
   `AppState::handleDigitalEdge` by adding a new pin constant and a branch that
   fires only on the rising edge. The existing hooks already juggle long/short
   presses; keep this one momentary-only so you never block the audio thread.
   The pattern lives around the reseed + lock handlers today.【F:src/app/AppState.cpp†L435-L498】
2. **Snapshot 2 s of input into a ring.** The granular engine already treats any
   non-SD source as live input and resolves paths from the short SD bank when a
   clip slot is requested.【F:src/engine/Granular.cpp†L352-L397】 Bolt a tiny sampler
   (even a mono buffer) into the capture ISR, tagged with a counter so each press
   lands in a deterministic slot like `captureIndex % shortBankSize`.
3. **Emit a reseed request with lineage breadcrumbs.** When the buffer closes,
   bump `masterSeed_` or stash a `captureCounter` and call `reseed(...)` so the
   scheduler walks the new genomes. The seed system already threads per-press
   lineage through the master seed; keep using that so cached seeds and engines
   stay in lockstep.【F:docs/roadmaps/seed_system.md†L7-L20】【F:src/app/AppState.cpp†L500-L520】
4. **Deterministic variation knobs instead of FIFO.** Instead of always picking
   the newest clip, derive the selection from a user-set variation mask: e.g. a
   `variationMode` enum that maps to `(captureCounter + kPrimeOffsets[mode]) %
   shortBankSize`. Feed that number into `seed.sampleIdx` so engines pull the
   same snapshot on every reroll unless the player tweaks the mode. Sprinkle
   spray/jitter via the seed PRNG if you want timing chaos without changing the
   deterministic clip choice.【F:src/engine/Granular.cpp†L376-L395】
5. **Surface it in the UI copy.** Add a Seed-page blurb that explains the button
   is a two-second grabber, not a transport toggle, and that variation modes are
   deterministic unless locks are set. The roadmap-style notes here double as
   the teaching aid for students wiring up their own enclosures.

Bottom line: the momentary button is just another seed source—capture a tiny
sample loop, pick its slot with PRNG math instead of chronological order, and
let reseeds keep the playback deterministic unless the performer dials in a new
variation profile.

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
- The same suite carries two fresh guardrails for the tap-tempo + preset prime
  flows so the prose above can link straight to proof:
  - `test_tap_tempo_prime_updates_lineage` averages the recorded tap intervals,
    asserts we tag every seed with the derived BPM lineage, and re-runs the
    prime to verify all granular params stay frozen so performers can trust
    consecutive taps won't mutate timbre.【F:tests/test_app/test_seed_prime_modes.cpp†L94-L133】
  - `test_preset_prime_applies_granular_params` hydrates a fake preset bank,
    flips locks + engine assignments, intentionally mutates unlocked seeds, and
    then reseeds to show we restore the preset's granular source/slot combo and
    lock state verbatim — the seed atlas's curated genomes stay canonical.
    【F:tests/test_app/test_seed_prime_modes.cpp†L135-L198】

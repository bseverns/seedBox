# Option A — Euclid + Burst metronome playbook

Two of the earliest engines — Euclid (deterministic pattern masks) and Burst
(clustered trigger sprays) — already live under `engine/Euclid.*` and
`engine/Burst.*`. The problem? Their story time has been trapped inside a unit
test for months. This page liberates the math and intent so you can wrap your
head around the pulse logic before spelunking the code.

> 🎯 **Why this duo?** They share the same scheduling backplane and often tag-team
> the same gigs. Euclid sets the macro pulse grid while Burst throws micro-flams
> across that scaffold. Get both in sync and you’ve got half a rhythmic toolkit.

## Euclid engine — building the mask

The Euclidean engine hands out regularly-spaced hits over a fixed number of
steps. The implementation leans on a deterministic compare instead of a fancy
Bjorklund heap, which keeps it portable and lets the simulator narrate every bit
flip.

- **Inputs** (`Param::kSteps`, `Param::kFills`, `Param::kRotate`): we quantise
  `fills` across `steps`, then wrap the mask by `rotate` positions *after*
  generation. That is why the “unrotated” base mask begins on index **two** in
  the tests — the compare kicks off at the third quantised division.
- **Mask layout:** the engine caches the mask in `EuclidEngine::mask()`.
  `tests/test_engine/test_euclid_burst.cpp` exercises three rotations so you can
  check your mental model against real bytes. Spin that file while reading this
  section; it’s the annotated lab notebook.【F:tests/test_engine/test_euclid_burst.cpp†L50-L95】
- **Tick path:** every `onTick` call advances a position counter, checks the
  cached mask, and exposes `lastGate()` for the router. Nothing random here —
  reseeding or rebooting keeps the groove intact.
- **State persistence:** `serializeState()` captures the mask + rotation so
  sessions survive swaps, and the tests restore into a fresh engine to prove it.
  No hidden dynamics, no “surprise, the mask mutated.”

### Euclid homework / gotchas

- Guard against zero-step configurations. The compare code already early-outs,
  but double-check if you add live UI later.
- Any future swing/shuffle layer should act *after* the mask. Keep this file’s
  deterministic core as the truth source.

## Burst engine — clustered trigger math

Burst takes a single seed trigger and explodes it into a deterministic stack of
micro-hits. Think `kClusterCount` clones spaced `kSpacingSamples` apart.

- **Parameter clamps:** `kClusterCount` stops at 16, even if the user dials 32.
  That limit shows up in the tests so we keep runtime bounded on Teensy as well
  as in the sim.【F:tests/test_engine/test_euclid_burst.cpp†L97-L144】
- **Spacing rules:** negative spacing collapses the stack into a flam — every
  trigger lands on the original timestamp. This keeps bad UI input from spraying
  backwards in time.
- **Pending trigger queue:** `BurstEngine::pendingTriggers()` is just a vector of
  absolute sample positions. Tests document the exact offsets so you can confirm
  the math when wiring hardware interrupts later.
- **State round-trips:** serialization preserves the cluster geometry. The test
  constructs a second engine, deserialises, and re-fires to prove we didn’t lose
  stride.

### Burst homework / gotchas

- Back-pressure: we still need guard rails so repeated seeds don’t flood the
  queue faster than the scheduler drains it. Keep an eye on `EngineRouter`
  integration if you add async inputs.
- Consider exposing per-hit velocity scaling once we have downstream voices that
  care. The mask is ready; only the payload is missing.

## Router + UI handshake

Both engines ride the same control plane:

1. `EngineRouter` assigns slots (Euclid lives at `EngineRouter::kEuclidId`, Burst
   at `EngineRouter::kBurstId`).
2. `EngineRouter::reseed` fans the master seed into each engine unless global or
   per-slot locks say otherwise — see the `run_router_reseed_and_locks` helper in
   the test for the canonical flow.【F:tests/test_engine/test_euclid_burst.cpp†L146-L185】
3. The display snapshot capture in the same test file shows how the UI surfaces
   status codes (`ECL`, `BST`). Those snapshots double as documentation artifacts
   when you flip `ENABLE_GOLDEN`.

## Field guide

- Want to see the groove? Run the Euclid/Burst unit test and skim the assert
  messages; they were written as prose on purpose.
- Need to demo it? Capture `AppState::DisplaySnapshot` with `ENABLE_GOLDEN=1` and
  drop the resulting `artifacts/engine_snapshots.txt` into the docs.
- Extending the math? Keep the deterministic compare + serialized mask as the
  sacred core. Build new flavor on top rather than rewriting the Euclid seed.

Welcome to the pulse lab. Keep it tight, keep it weird, and document your math
like you’re leaving clues for your future self.

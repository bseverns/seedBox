# Native Fixture Sound Playbook

This playbook translates the generated native golden WAV fixtures into musical
SeedBox moves. The fixtures are regression proofs, not presets. The goal for a
player is to reproduce the gesture and role of the sound: grain cloud, resonant
tail, Euclid mask, burst cluster, hybrid stack, spatial bus, or reseed drift.

The fixture source of truth is `tests/native_golden/golden.json`; regenerated
audio lands under `build/fixtures/`. Do not commit ad-hoc generated WAVs unless
you are intentionally refreshing checked-in golden material.

## Audition Loop

Render or refresh the native fixtures:

```bash
pio test -e native_golden
```

Offline fallback:

```bash
./scripts/offline_native_golden.sh
```

Browse local fixtures:

```bash
python3 scripts/serve_golden_fixture_browser.py
```

## Reading A Recipe

- **Fixture:** generated WAV name in `build/fixtures/`.
- **Musical target:** what a user should hear.
- **Machine move:** the engine, seed-slot, control, and routing choices that get close.
- **Listen for:** the quality that makes the patch successful.

Exact hashes are for tests. Human success is matching the musical behavior.

## Core Engine Recipes

### `drone-intro`

**Musical target:** a plain 110 Hz mono wake-up tone.

**Machine move:**
- Use this only as a reference that the audio path is alive.
- Start with a simple oscillator/sample tone.
- Keep modulation, spread, reseed, and scheduler movement off.

**Listen for:** stable level, no crackle, no missing output.

### `sampler-grains`

**Musical target:** stacked sampler voices around root, fifth, and ninth.

**Machine move:**
- Engine: Sampler.
- Use one seed as the root anchor.
- Add two nearby pitch offsets: fifth and ninth.
- Keep density moderate so individual grains are still readable.
- Add a little detune or spread, but keep the chord identity intact.

**Listen for:** a stable chord stack, not a smear. Detune should add width
without hiding the interval structure.

### `modulated-sampler`

**Musical target:** sampler voice with audible macro movement.

**Machine move:**
- Engine: Sampler.
- Start from a short tonal source.
- Move tone, envelope contour, or pitch in a slow lane.
- Keep probability high enough that modulation is easy to hear.
- Use stereo spread if the source feels too centered.

**Listen for:** a sampled voice that changes shape over time instead of
repeating as a static one-shot.

### `granular-haze`

**Musical target:** stereo grain cloud with slow swirl.

**Machine move:**
- Engine: Granular.
- Source: live input or a sustained sample.
- Grain size: medium to long.
- Spray: moderate to high.
- Stereo spread: high.
- Density: medium.
- Tone: bright enough to keep grain edges audible.
- Movement: slow tone, spread, source-position, or macro-orbit nudges.

**Listen for:** wide cloud, audible motion, and no static mono wash.

### `resonator-tail`

**Musical target:** plucked resonant body with a shaped decay.

**Machine move:**
- Engine: Resonator.
- Input: short transient, pluck, tap, or percussive sample.
- Feedback: medium-high.
- Damping: lower for longer ring; higher for tighter body.
- Brightness: raise until the attack speaks, then back off if it whistles.
- Bank/mode: choose a tuned body that supports the source pitch.

**Listen for:** clear attack followed by a musical tail. Damping should change
the decay, not just the volume.

## Rhythm And Trigger Recipes

### `euclid-mask`

**Musical target:** gated stereo Euclid rhythm with pan/envelope movement.

**Machine move:**
- Engine: Euclid.
- Set a sparse-to-medium hit count.
- Keep probability high while learning the mask.
- Add swing only after the basic pattern is clear.
- Move pan/spread per hit or with macro orbit.
- Use a short envelope if the rhythm feels smeared.

**Listen for:** a repeatable mask. The pattern should feel intentional, not
like random triggering.

### `burst-cluster`

**Musical target:** grouped trigger flams with audible spacing.

**Machine move:**
- Engine: Burst.
- Density: medium-high.
- Probability: high.
- Spacing/jitter: enough to hear separate hits inside the cluster.
- Tone: bright enough to expose the leading edges.
- Spread: low for a tight cluster; higher if it needs width.

**Listen for:** clustered bursts with internal spacing, not a flat roll.

### `layered-euclid-burst`

**Musical target:** Euclid pulse bed with Burst accents layered on top.

**Machine move:**
- Slot 1: Euclid for the repeating grid.
- Slot 2: Burst for fills or flam accents.
- Keep the Euclid layer steadier and the Burst layer more episodic.
- Let density or probability separate the two roles.
- Pan the accent layer away from the pulse bed if the mix gets crowded.

**Listen for:** a base rhythm plus accent behavior. If the layers blur into
one pattern, reduce Burst density or shorten its envelope.

## Hybrid Stack Recipes

### `engine-multi-ledger`

**Musical target:** Euclid-driven sampler, resonator, and granular crossfade
with per-hit macro orbit.

**Machine move:**
- Slot 1: Sampler for attack and pitch anchor.
- Slot 2: Resonator for body and decay.
- Slot 3: Granular for halo or smear.
- Scheduler: Euclid hits drive the phrase.
- Macro: orbit pan and density per hit.
- Crossfade or level-balance the layers so each hit has attack, body, and air.

**Listen for:** one rhythmic phrase with three timbral stages, not three
unrelated instruments.

### `engine-hybrid-stack`

**Musical target:** sampler/resonator/granular stack driven by Euclid/Burst
schedule with macro pan and modulation lanes.

**Machine move:**
- Slot 1: Sampler for tonal anchor.
- Slot 2: Resonator for tuned body.
- Slot 3: Granular for texture.
- Slot 4: Euclid or Burst as the trigger personality.
- Use macro pan to keep the stack moving in stereo.
- Keep density balanced: sampler clear, resonator sustained, granular behind.

**Listen for:** separate layers sharing one groove. The stack should feel
arranged, not simply louder.

### `engine-macro-orbits`

**Musical target:** hybrid stack with automation lanes sweeping tone, tilt,
density, and stereo position.

**Machine move:**
- Build the `engine-hybrid-stack` first.
- Add slow macro lanes:
  - sampler contour or crunch
  - resonator damping or brightness
  - granular spray
  - global pan/orbit
- Keep modulation slow enough to read as an orbit, not a wobble.

**Listen for:** evolving perspective. The phrase should travel around the mix
while the core rhythm stays understandable.

### `mixer-console`

**Musical target:** every engine present in a controlled stereo console mix.

**Machine move:**
- Bring up each engine one at a time.
- Keep peaks below clipping; aim for headroom before adding the next layer.
- Use pan/spread to separate roles.
- Treat density like level: too much density can overload the bus even if
individual seed levels feel reasonable.

**Listen for:** all engines audible without the bus collapsing. This is a mix
sanity target, not a featured patch.

## Reseed And Evolution Recipes

### `long-random-take`

**Musical target:** deterministic long-form variation.

**Machine move:**
- Start from a playable seed state.
- Use a fixed master seed.
- Let reseed or seed-prime modes change the world over time.
- Keep probability and density high enough to reveal the changes.
- Avoid over-editing during the take; let the machine expose the sequence.

**Listen for:** changing character that still feels repeatable. If it feels
random in a bad way, lower mutation or density.

### `reseed-A`, `reseed-B`, `reseed-C`

**Musical target:** short deterministic reseed studies.

**Machine move:**
- Pick one seed-prime mode.
- Reseed from a known master seed.
- Listen to one family at a time:
  - `A`: stable baseline world
  - `B`: brighter or denser contrast world
  - `C`: more transformed contrast world
- Keep the same input and clock so the reseed is the variable.

**Listen for:** each reseed should have a recognizable identity. If A/B/C feel
interchangeable, increase contrast in engine, density, tone, or source choice.

### `reseed-poly`

**Musical target:** poly-world reseed behavior.

**Machine move:**
- Use multiple seed slots.
- Give each slot a different engine role.
- Reseed while preserving the overall clock and input context.
- Let the focused slot change character while support slots keep continuity.

**Listen for:** transformation across multiple voices without losing the
thread of the piece.

## Spatial And Bus Recipes

### `quad-bus`

**Musical target:** four-channel placement proof.

**Machine move:**
- Treat outputs as front-left, front-right, rear-left, rear-right.
- Use spread and pan/orbit to place voices deliberately.
- Keep one dry or anchor element near the front.
- Put texture or tails in the rear channels.

**Listen for:** placement, not just stereo made louder. Each channel should
have a reason to exist.

### `surround-bus`

**Musical target:** six-channel surround movement.

**Machine move:**
- Keep the main musical phrase in the front pair.
- Use surrounds for grains, tails, and motion.
- Avoid putting every engine everywhere.
- Use LFE sparingly if the routing exposes it.

**Listen for:** a stable front image with movement around it.

### `stage71-bus`

**Musical target:** 7.1 lane naming and stage crossfeed proof.

**Machine move:**
- Think in lanes: L, R, C, LFE, Ls, Rs, Lrs, Rrs.
- Put center-worthy material in C only when it needs focus.
- Keep LFE as support, not a full-range dump.
- Let rear lanes carry space, echoes, or granular air.

**Listen for:** channel identity. This fixture is about routing confidence as
much as musical tone.

## Contract And Receipt Follow-Up

After the playbook, the useful next proof is a small Markdown receipt from a
local full `golden-permutations` external-input run. Commit only summary
metrics and pass/fail notes; keep generated WAVs and local absolute paths out
of git.

Add a future `contract_tuning_notes.md` beside the scenario manifest when you
start listening against the musical contracts. Each entry should record:

- date
- input source
- scenario
- metric result
- listening judgment: too loose / too tight / musically meaningful
- proposed threshold change, if any

Longer term, split contracts into two levels:

- `ci_contract`: conservative, stable, non-flaky
- `release_contract`: stricter, listening-informed, musically meaningful

CI should stay gentle. Release candidates should be harder to please.

# Seed Gallery

SeedBox’s deterministic renders are not just engineering leftovers. They are a
public listening surface.

This repo uses golden fixtures to capture repeatable renders and matching
control ledgers. Think of them as receipts: if an engine, scheduler rule, or
modulation lane changes, the repo can point to a concrete artifact instead of
asking reviewers to trust memory.

## Why the gallery matters

For engineering, the gallery shows that changes can be diffed against a known
artifact set.

For listening and teaching, the gallery turns tests into study material. A
fixture is a small lesson: here is a sound, here is the control transcript that
produced it, and here is the test or helper that minted it.

## What lives in the gallery today

The current native golden set includes small reference captures, engine studies,
mix/bus studies, and longer deterministic takes. The manifest in
[`tests/native_golden/golden.json`](../tests/native_golden/golden.json) includes
fixtures such as:

- `drone-intro`
- `burst-cluster`
- `euclid-mask`
- `granular-haze`
- `mixer-console`
- `layered-euclid-burst`
- `engine-hybrid-stack`
- `engine-macro-orbits`
- `engine-multi-ledger`
- `long-random-take`

Many of these also ship with matching `*-control.txt` ledgers so the render can
be read as well as heard.

## Current fixture index

| Fixture | What it is for | What to listen for | Companion ledger |
| --- | --- | --- | --- |
| `drone-intro` | Minimum DSP wake-check | obvious pitch level and continuity changes | `drone-intro-control.txt` |
| `sampler-grains` | Sampler voicing reference | voicing, detune, and grain-stack balance drift | `sampler-grains-control.txt` |
| `resonator-tail` | Resonator damping/feedback reference | tail length, decay shape, and feedback color shifts | `resonator-tail-control.txt` |
| `granular-haze` | Granular width/sync reference | stereo width slips and swirl/sync changes | `granular-haze-control.txt` |
| `mixer-console` | Multi-engine bus sanity check | overall balance and obvious bus-level shifts | `mixer-console-control.txt` |
| `euclid-mask` | Euclid pattern audibility check | gate spacing, pan motion, and envelope-shape changes | `euclid-mask-control.txt` |
| `burst-cluster` | Burst spacing reference | cluster spacing and density changes | `burst-cluster-control.txt` |
| `layered-euclid-burst` | Shared-schedule engine layering study | shared timing behavior across sampler/resonator/granular layers | `layered-euclid-burst-control.txt` |
| `modulated-sampler` | Sampler/granular automation stress case | fast automation wobble and modulation-lane drift | `modulated-sampler-control.txt` |
| `engine-hybrid-stack` | Multi-engine deterministic track sheet | cross-engine timing or modulation-lane drift | `engine-hybrid-stack-control.txt` |
| `engine-macro-orbits` | Macro modulation pedagogy pass | orbit/pan movement, density sweep, and macro contour changes | `engine-macro-orbits-control.txt` |
| `engine-multi-ledger` | Per-hit macro ledger study | hit-by-hit crossfade/orbit/density changes | `engine-multi-ledger-control.txt` |
| `quad-bus` | 4-channel routing proof | obvious lane routing mistakes and front/rear balance changes | `quad-bus-control.txt` |
| `surround-bus` | 6-channel routing proof | center/LFE/surround balance shifts and lane assignment mistakes | `surround-bus-control.txt` |
| `stage71-bus` | 7.1 routing proof | named-lane placement drift across the 8-channel layout | `stage71-bus-control.txt` |
| `reseed-A` / `reseed-B` | Baseline reseed references | obvious reseed schedule or event-shape changes | matching `*-control.txt` files |
| `reseed-C` | Higher-density reseed stress case | tempo-locked density and swing behavior changes | `reseed-C-control.txt` |
| `reseed-poly` | Overlapping-pluck/poly layering study | overlap behavior between the extra sampler and resonator lanes | `reseed-poly-control.txt` |
| `long-random-take` | Long-form deterministic regression listen | slow-burn drift or cumulative behavior changes over 30 seconds | `long-random-take-control.txt` |

## Where to go next

- [Native golden roadmap](roadmaps/native_golden.md)
- [Native golden harness README](../tests/native_golden/README.md)
- [Test harness guide](../tests/README.md)
- [Examples](../examples)
- [Tutorials](tutorials)

## How to think about "golden fixtures"

Golden fixtures are not claims of perfection. They are repeatable reference
artifacts.

They answer questions like:

- What did this engine render before the change?
- Did the timing ledger move?
- Did the modulation transcript change with intent?
- Can a reviewer inspect both the sound and the control story?

That makes them useful as both engineering proof and listening pedagogy.

## Gallery surfaces already in the repo

- Artifact catalog: [`tests/native_golden/golden.json`](../tests/native_golden/golden.json)
- Checked-in fixture crate: [`build/fixtures/README.md`](../build/fixtures/README.md)
- Render/test code: [`tests/native_golden/test_main.cpp`](../tests/native_golden/test_main.cpp)
- Helper renders: [`tests/native_golden/wav_helpers.cpp`](../tests/native_golden/wav_helpers.cpp)
- Broader proof framing: [`docs/StabilityAndSupport.md`](StabilityAndSupport.md)
- UI-oriented visual surface: [`docs/ui_ascii_gallery.md`](ui_ascii_gallery.md)

## TODOs

- TODO: add future screenshots or waveform views only when they are generated
  from repo artifacts, not staged as fictional marketing media.
- TODO: add a small hardware-vs-native comparison gallery once repeatable bench
  captures exist.

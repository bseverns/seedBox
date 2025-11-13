# Scale quantizer flow — knob motion to snapped pitch

Welcome to the quantizer lab. The roadmap already teased `util::ScaleQuantizer`
as the glue between UI experiments and engine brains. This tutorial turns that
promise into a repeatable walk-through: read the wiring, run a tiny test, and
fire up a demo executable that behaves like a ghost front panel.

## TL;DR

- `AppState::applyQuantizeControl` sanitises an incoming CC nibble, maps it to a
  `util::ScaleQuantizer::Scale`, and writes the snapped pitch back to the focused
  seed before rebroadcasting to the engines.【F:src/app/AppState.cpp†L1509-L1551】
- `util::ScaleQuantizer` exposes three hooks (`SnapToScale`, `SnapUp`,
  `SnapDown`) so UI code can pick the directionality that fits the story.
- This doc ships with a deterministic unit test and an executable demo you can
  run from the repo right now. Nothing hand-wavy.

## 1. Map the pipeline

The front-panel quantize control (MN42 CC 18 in the field notes) encodes both
scale and root in a single byte. The firmware does three quick moves:

1. Split the byte into `scaleIndex = value / 32` and `root = value % 12`.
2. Clamp each field so wild controllers or tests never crash the UI.
3. Reach for `util::ScaleQuantizer::SnapToScale` with the chosen scale enum.

`AppState` keeps the current scale + root cached so the display can mirror the
selection. If the focused seed is locked, the method returns early – the quantize
knob is polite and respects student-crafted genomes.

### Snap helpers at a glance

```c++
// header lives in include/util/ScaleQuantizer.h
static float SnapToScale(float pitch, std::uint8_t root, Scale scale);
static float SnapUp(float pitch, std::uint8_t root, Scale scale);
static float SnapDown(float pitch, std::uint8_t root, Scale scale);
```

All three variants accept a semitone offset (the seed pitch value), the scale
root (0–11, modulo’d internally), and a scale enum. The implementation walks
across a ±2 octave window to find the closest valid degree. `SnapUp` and
`SnapDown` filter candidates so the delta always moves in the intended direction,
making “resolve up” or “gravity down” controls trivial to wire.

## 2. Run the regression test

A unit test lives under `tests/test_util` to document expectations in code. It
covers the three snapping modes and a couple of boundary cases (negative roots,
ties across octaves).

```bash
pio test -e native --filter tests/test_util/test_scale_quantizer.cpp
```

Read the assertions while the command runs; the test narrates why each scenario
matters so future UI experiments have guardrails.

## 3. Play with the demo harness

The new example in `examples/04_scale_quantizer/` acts like a virtual quantize
knob. It ships with a CLI that mirrors the firmware encoding:

```bash
cd examples/04_scale_quantizer
pio run -e native
.pio/build/native/program --scale=minor --root=9 --mode=up --offsets=-3.7,-0.8,0.2,4.6
```

What you get back is a narrated table: the incoming offsets, the snapped pitches
for each mode, and how the chosen root shifts the scale. Think of it as a
standalone teaching prop – perfect for classes where you want to audition scale
behaviour before touching the panel.

Under the hood the CLI leans on a tiny `QuantizeHarness` class that mirrors the
firmware call sites: cache the selected scale/root, pick a direction (nearest,
up, down), then dispatch to `ScaleQuantizer`. You can lift that class straight
into prototype UI code without rewriting any math.

## 4. Extend it

- Want to script scale rotations or morph between roots? The harness already
  exposes setters; wrap them in your favourite scripting language.
- Need to visualise the snapped melody? Pipe the CLI output into a plotter or
  drop the numbers into a DAW automation lane. The output stays deterministic so
  tests and docs can reference exact values.
- Dreaming of hardware knobs with “always resolve up” behaviour? Swap the demo
  to `--mode=up`, feed it incoming offsets from your sensor, and forward the
  results to the engine the same way `AppState` does today.

Scale quantizing should feel like a playground, not a black box. These receipts
keep it that way.

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
- This doc ships with deterministic regression tests *and* a demo harness that
  can export CSVs, apply a slow drift LFO, and stream frames into the UI sim.

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

A unit test under `tests/test_util` documents expectations in code. It now covers
all three snapping modes, root wrapping edge cases, and a drift/CSV regression so
future docs have guardrails.

```bash
pio test -e native --filter test_util
```

Watch for the `test_scale_quantizer_drift_samples_and_csv` case – it verifies the
sine LFO math and asserts that the CSV header stays stable for downstream tools.【F:tests/test_util/test_scale_quantizer.cpp†L57-L104】

## 3. Export quantized data

The demo in `examples/04_scale_quantizer/` acts like a virtual quantize knob and
speaks fluent CSV.

```bash
cd examples/04_scale_quantizer
pio run -e native
.pio/build/native/program --scale=minor --root=9 --mode=up \
  --offsets=-3.7,-0.8,0.2,4.6 --export-csv
```

Console output still narrates the `t=0` snapshot, and the harness writes the
full table to `out/scale_quantizer.csv` with a header row and one line per
slot/frame.【F:examples/04_scale_quantizer/src/main.cpp†L295-L306】【F:examples/04_scale_quantizer/README.md†L24-L44】

## 4. Wobble the offsets with drift

Add `--drift=<Hz>` to project a slow sine wobble (depth ±0.45 semitones) across
all offsets. The harness renders one complete cycle in 17 frames, so you can see
both crest and trough in the exported data.【F:examples/04_scale_quantizer/src/main.cpp†L180-L228】【F:include/util/ScaleQuantizerFlow.h†L11-L33】

```bash
.pio/build/native/program --drift=0.25 --export-csv=out/drift_demo.csv
```

The CSV grows to include `time_sec`, `slot`, `drifted_pitch`, and the snapped
values for every mode.【F:src/util/ScaleQuantizerFlow.cpp†L61-L99】 Drop it into a
spreadsheet, a DAW, or your favourite plotting tool to watch the quantizer breathe.

## 5. Drive the UI sim

The harness can stream each frame either as OSC (`--osc=host:port`) or JSON over
WebSocket (`--ws=ws://host:port/path`). A lightweight console widget ships in
`scripts/native/quantizer_ws_display.py`; it only needs Python’s standard library.

```bash
./scripts/native/quantizer_ws_display.py --port 8765  # terminal 1
.pio/build/native/program --ws=ws://127.0.0.1:8765/quantizer --drift=0.5 \
  --offsets=-5.5,-1.2,0.3,2.6,7.8                             # terminal 2
```

The widget redraws in place with slot, drifted pitch, snapped outputs, and the
active mode – perfect for projecting alongside the firmware UI during a class demo.【F:scripts/native/quantizer_ws_display.py†L1-L108】 Swap the WebSocket flag for
`--osc=127.0.0.1:9000` if you prefer to feed an OSC-aware visualiser.

Scale quantizing should feel like a playground, not a black box. These receipts
keep it that way.

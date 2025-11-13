# 04 · scale quantizer — ghost knob rehearsal

The fourth quiet-mode vignette spotlights the `util::ScaleQuantizer` helpers.
Instead of staring at firmware comments, you can spin a pretend quantize knob
from the command line and watch pitches snap into place. Now it also throws
quantized CSVs, breathes a slow drift LFO, and streams raw frames to an OSC
listener or the classroom UI sim.

## What it does

* Targets PlatformIO's native environment, so `pio run -e native` inside this
  folder builds a runnable CLI harness.
* Mirrors the firmware's control byte: you pick `--scale`, `--root`, and `--mode`
  (nearest/up/down) to match the front-panel quantize behaviour.
* Prints a narrated table so workshops can audition scale + direction changes
  without touching hardware.

## Controls

```
.pio/build/native/program --scale=<chromatic|major|minor|penta-major|penta-minor> \
  --root=<0-11> --mode=<nearest|up|down> --offsets=<comma-separated floats> \
  [--drift=<Hz>] [--export-csv[=out/<file>]] [--osc=host:port] \
  [--ws=ws://host:port/path]
```

* `--scale` — which `ScaleQuantizer::Scale` enum to mirror.
* `--root` — root note in semitones (the harness wraps it to 0–11 just like the
  firmware).
* `--mode` — choose between nearest note (`SnapToScale`), force-up (`SnapUp`),
  or force-down (`SnapDown`).
* `--offsets` — semitone offsets you want to quantize. Feed negatives,
  fractional values, whatever melody scrap you're workshopping.
* `--drift` — optional LFO in Hz. When set, the harness renders a full sine
  cycle (depth ±0.45) so you can watch the quantizer breathe over time.
* `--export-csv` — drop a CSV into `out/`. Pass a custom relative filename with
  `--export-csv=out/my_take.csv`.
* `--osc` — fire each sample at an OSC endpoint (`host:port`). Messages land at
  `/quantizer/sample` with slot/time/pitch payloads.
* `--ws` — stream JSON frames to a WebSocket sink (perfect for the UI sim).

Run with no flags to see the default C major, "nearest" walkthrough.

## Capture a CSV

```
cd examples/04_scale_quantizer
pio run -e native
.pio/build/native/program --export-csv --scale=minor --root=9 --mode=up \
  --offsets=-3.7,-0.8,0.2,4.6
```

The harness writes `out/scale_quantizer.csv` (or whatever `out/<file>` you
named) with a header row and one line per slot/frame. The repo enforces the `out/`
sandbox so nothing sneaks outside the tree.

## Slow-drift clinic

```
.pio/build/native/program --drift=0.25 --export-csv=out/drift_demo.csv
```

You still get the narrated table for `t=0`, plus 17 frames that ride a ±0.45
sine wobble over one full cycle. CSV rows include the raw input, drifted pitch,
and snapped output for every slot so students can plot the envelope or feed the
numbers straight into a DAW.

## Feed the UI sim

1. Launch the console widget:
   ```
   ./scripts/native/quantizer_ws_display.py --port 8765
   ```
2. Point the harness at it:
   ```
   .pio/build/native/program --ws=ws://127.0.0.1:8765/quantizer --drift=0.5
   ```

The widget redraws in place with each JSON frame: slot index, drifted pitch,
and the active mode. Swap `--ws` for `--osc=127.0.0.1:9000` if you want to
drive an OSC-aware UI (or sniff messages with `oscdump`).

Quantizing should feel playful, so treat this harness like a pedalboard: stomp
on it, hear the chord resolve, tweak again—and now capture the proof, too.

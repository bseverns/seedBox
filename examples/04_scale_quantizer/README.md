# 04 · scale quantizer — ghost knob rehearsal

The fourth quiet-mode vignette spotlights the `util::ScaleQuantizer` helpers.
Instead of staring at firmware comments, you can spin a pretend quantize knob
from the command line and watch pitches snap into place.

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
  --root=<0-11> --mode=<nearest|up|down> --offsets=<comma-separated float list>
```

* `--scale` — which `ScaleQuantizer::Scale` enum to mirror.
* `--root` — root note in semitones (the harness wraps it to 0–11 just like the
  firmware).
* `--mode` — choose between nearest note (`SnapToScale`), force-up (`SnapUp`),
  or force-down (`SnapDown`).
* `--offsets` — semitone offsets you want to quantize. You can feed negatives,
  fractional values, whatever melody scrap you're workshopping.

Run with no flags to see the default C major, "nearest" walkthrough.

## Wiring

Still zero soldering. This stays a native-only lab so you can iterate with just a
shell prompt and a curious class.

## TODO for future embellishments

* Capture the snapped melody into `/out/scale-quantizer.csv` for DAW import.
* Add a `--drift` flag that animates a slow LFO over the offsets so you can watch
  resolve-up/down over time.
* Pipe the harness into the UI sim once the display widget lands so students can
  watch the OLED change while the math stays in sync.

Quantizing should feel playful, so treat this harness like a pedalboard: stomp
on it, hear the chord resolve, tweak again.

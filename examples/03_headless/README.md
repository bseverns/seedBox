# 03 · headless — automation in the void

For the third quiet-mode vignette we ditch even the pretend UI and drive everything via callbacks. It's a dry run for scripting automation when the SeedBox brain is bolted under a desk with no screen and no amp.

## What it does

* Targets PlatformIO's native environment, so running `pio run -e native` inside this folder just works.
* Schedules a headless loop that ticks silent automation lanes for filter, delay, and VCA moves.
* Proves we can choreograph control voltage ideas without crackling a single buffer.

## Wiring

Still nothing to solder. If your speakers woke up during this demo, something is haunted.

## TODO for future WAV renders

* Dump the automation curves into `/out/headless-automation.wav` once the render backend ships.
* Add a JSON trace companion for DAW import—call it `/out/headless-automation.json`.
* Stress the loop with 10x the lanes and make sure the quiet-mode throttling holds.

Silence is the most rebellious fader position.

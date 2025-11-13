# 03 · headless — automation in the void

For the third quiet-mode vignette we ditch even the pretend UI and drive everything via callbacks. It's a dry run for scripting automation when the SeedBox brain is bolted under a desk with no screen and no amp.

## What it does

* Targets PlatformIO's native environment, so running `pio run -e native` inside this folder just works.
* Schedules a headless loop that ticks silent automation lanes for filter, delay, and VCA moves.
* Pipes every lane into the shared offline renderer so those silent pokes become resonator pings.
* Can spit out both `out/headless-automation.wav` and `out/headless-automation.json` for DAWs and notebooks.

## Wiring

Still nothing to solder. If your speakers woke up during this demo, something is haunted.

## Bouncing automation receipts

Run it like the other native sims: build, then call the binary with `--export` to generate both artifacts.

```bash
cd examples/03_headless
pio run -e native
./.pio/build/native/program --export
```

You’ll find `out/headless-automation.wav` alongside a matching JSON trace. The `out/` folder is still gitignored, so you can stash these takes without polluting history. Silence is the most rebellious fader position—and now it comes with proof.

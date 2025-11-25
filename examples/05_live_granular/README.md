# 05 · live grains — ghosting the mic lane

Tiny sketch to show how the "live input" granular lane gets rehearsed without
waking the DAC. We fake live grains as sampler triggers, sprinkle modal echoes
on the downbeats, and optionally bounce the whole mess into `out/` with a single
flag. Same teaching vibe as `01_sprout`, just a little grittier.

## What it does

* Compiles as a native PlatformIO target so you can run it straight on a laptop.
* Scripts a handful of faux live-input grains with controllable spray and count.
* Tags every fourth grain for a resonator "double dip" to hear how modal tails
  stitch onto the cloud.
* Can keep chatter down during exports via `--quiet-export` while still
  respecting the offline renderer pipeline.

## Controls

* `--grains=<n>` — how many grains to schedule before bailing (default: 18).
* `--spray-ms=<ms>` — timing jitter in milliseconds applied per grain
  (default: 22.0).
* `--export-wav[=<path>]` — bounce the ghost take into `out/live-grains.wav` or
  a custom path you pass in.
* `--quiet-export` — silence the per-grain log spam while still rendering and
  exporting.

## Running it

```bash
cd examples/05_live_granular
pio run -e native
./.pio/build/native/program --export-wav
```

When `--export-wav` is set, the renderer writes to `out/live-grains.wav` by
default. Pass `--export-wav=out/alt-grains.wav` if you want to keep multiple
bounces around. `out/` is already gitignored, so you can stash as many takes as
you need.

## Why bother

* Mirrors the **granular live-input** planning doc so you can trace the sim back
  to the roadmap: [`docs/roadmaps/granular.md`](../../docs/roadmaps/granular.md).
* Hooks into the same offline renderer golden harness that guards the reseed
  demos: peek at [`tests/native_golden`](../../tests/native_golden) to see how
  WAV fixtures get validated.
* Keeps the punk-rock ethos: build, bounce, inspect the WAV, and tweak the seed
  script without ever plugging in hardware.

Treat this README as a notebook. Scribble why you changed a grain recipe, not
just the numbers you nudged.

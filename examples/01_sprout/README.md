# 01 · sprout — first quiet pulse

This sketch is the "hello world" for SeedBox's quiet-mode sandbox. It's a sequencer stub that scribbles out beat markers without ever arming the audio stack. Think of it like tuning a synth with the amp muted: we're mapping intent before we start pushing air.

## What it does

* Boots into a native PlatformIO build that runs on your laptop/desktop.
* Flags quiet-mode as `true`, then simulates two measures of 4/4 at 96 BPM.
* Pipes every ghost hit through the shared offline renderer so sampler and resonator pings actually stack up.
* Can bounce the render straight into `out/intro-sprout.wav` without ever touching the DAC.

## Controls

The sim now ships with a couple quick toggles so you can poke at the groove without recompiling:

* `--quiet` / `--loud` — stick with the fast-forward ghost clock or stretch back to full-length beats.
* `--mutate=<name>` — flip between the baked-in kits (`default`, `hatless`, `afterbeat-chop`).
* `--export-wav` — render the quiet take into `out/intro-sprout.wav` before quitting.
* `--list-mutations` — print whatever ghost kits have been scripted.

Mix and match them through PlatformIO or by compiling the translation unit with a standard `g++` toolchain while we iron out the official pipeline.

## Wiring

Nada. This is a pure-sim loop—no Teensy, no breakout, no speakers. Let your USB cable nap.

## Bouncing the ghosts

Want receipts? Build with PlatformIO and then punt the native binary through the new flag:

```bash
cd examples/01_sprout
pio run -e native
./.pio/build/native/program --export-wav
```

The renderer writes `out/intro-sprout.wav` and the folder stays gitignored, so your repo stays clean while your ears get proof.

# 01 · sprout — first quiet pulse

This sketch is the "hello world" for SeedBox's quiet-mode sandbox. It's a sequencer stub that scribbles out beat markers without ever arming the audio stack. Think of it like tuning a synth with the amp muted: we're mapping intent before we start pushing air.

## What it does

* Boots into a native PlatformIO build that runs on your laptop/desktop.
* Flags quiet-mode as `true`, then simulates two measures of 4/4 at 96 BPM.
* Drops log breadcrumbs instead of lighting up the DAC so you can watch the groove without waking the neighbors.

## Controls

The sim now ships with a couple quick toggles so you can poke at the groove without recompiling:

* `--quiet` / `--loud` — stick with the fast-forward ghost clock or stretch back to full-length beats.
* `--mutate=<name>` — flip between the baked-in kits (`default`, `hatless`, `afterbeat-chop`).
* `--list-mutations` — print whatever ghost kits have been scripted.

Mix and match them through PlatformIO or by compiling the translation unit with a standard `g++` toolchain while we iron out the official pipeline.

## Wiring

Nada. This is a pure-sim loop—no Teensy, no breakout, no speakers. Let your USB cable nap.

## TODO for future WAV renders

* Swap the console prints for real sampler triggers once we finish the render pipeline.
* Capture the ghosted hits into `/out/intro-sprout.wav` after the audio bus is wired up.
* Cross-check timing jitter against the hardware clock before blessing the take.

Until then, keep it quiet and scribble fearless.

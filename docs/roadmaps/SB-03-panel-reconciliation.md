# SB-03 — Reconcile the physical panel descriptions

Baseline: the Board layout implemented after SB-01. No separate SB-02 contract
was present when this work began, so this reconciliation preserves that layout
and records its inventory in `include/hal/PanelControls.h`. A future physical
layout decision should update that table and the linked builder surfaces.

## One current inventory

- Four encoder/switch assemblies: Seed/Bank, Density, Tone/Tilt, FX/Mutate.
- Four standalone momentary buttons: Tap Tempo, Shift, Alt Seed, Live Capture.
- Reseed is a hold on the Seed/Bank switch. There is no dedicated Reseed or Lock
  switch in the current Board mapping. Lock state remains available through
  application APIs and remote controls.

`PanelControls.h` supplies labels, native script tokens, and pin assignments to
TeensyBoard, NativeBoard, and the JUCE panel. `Board.h` describes the same eight
switches. The BOM now specifies four encoders, four knobs, and four standalone
buttons. The builder primer, SVG, panel cheat sheet, Storage/Seed roadmaps, and
HAL tutorial describe the same inventory and gestures. The OLED Storage hints
name Seed instead of GPIO.

## Desktop interaction

The JUCE panel previously exposed host-specific knob edits and separate Reseed
and Lock buttons. It now feeds encoder turns and switch edges through Board and
the existing InputEvents/page policy. Drag turns an encoder; right-click and
hold operates its integrated switch. Tap Tempo and Live Capture use the same
press/hold gestures as firmware. Shift and Alt Seed remain momentary modifiers.

The host maintenance timer enables elapsed Board time, so a physical-duration
hold does not depend on the desktop timer rate. Native scripted tests still
start with deterministic 10 ms steps. Mouse edges are sampled immediately so
quick clicks cannot vanish between maintenance timer callbacks.

Host keyboard shortcuts, automation parameters, and the optional legacy editor
remain desktop extensions; they are not additional physical panel controls.
The old panel-specific Alt+Reseed / Alt+Lock quick-preset gestures are replaced
by the shared Storage gestures on Alt Seed and Seed/Bank.

## Validation recorded for SB-03

- Native application suite: **68/68 passed**, including elapsed-clock mode and
  restoration of deterministic scripted timing.
- Teensy firmware: `pio run -e teensy40` **passed**; no device was flashed.
- JUCE standalone: CMake `SeedboxApp` target **passed** using the pinned JUCE
  dependency and the local Apple compiler/SDK paths.
- Temporary offscreen JUCE probe: verified four knobs/four named buttons, all
  four encoder-switch mode transitions, a Seed encoder turn, Seed hold reseed,
  and Live Capture press. Rendered the panel without opening an audio device.
- SVG and JUCE panel images were visually inspected; OLED and diagnostic label
  overlap found during inspection was corrected.
- `git diff --check`: passed.

Build directories, logs, and image probes were kept under `/tmp`; no compiled
artifacts were added to the repository. Physical operation and DAW automation
parity were not exercised by these checks.

## Follow-up

[SB-04](SB-04-panel-contract-tests.md) now enforces this inventory with pin
uniqueness, count, documentation, and independent-control checks. These steps
do not claim bench validation or a fabrication-ready panel drawing.

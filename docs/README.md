# SeedBox documentation staging ground

Welcome to the paper trail. If the firmware repo is the synth, this folder is
the zine you flip through on the bus ride to the gig. Every doc aims to teach,
not intimidate.

## What's inside

| File or folder | What you'll learn | Best time to read |
| --- | --- | --- |
| `builder_bootstrap.md` | Environment setup, wiring diagrams, lab ideas. | When you're booting hardware or helping a friend get started. |
| `assumptions.md` | Design bets, non-negotiables, and why quiet mode exists. | Before you ship a change that bends the vibe. |
| `juce_build.md` | JUCE desktop wiring, sample-rate/block-size assumptions, and how to keep Arduino headers out of the build. | Before you fire up a DAW build or headless JUCE test harness. |
| `ethics.md` | Privacy + data handling pact for labs and jams. | Any time you add IO, logging, or persistence. |
| `hardware_bill_of_materials.md` | Shopping list + sourcing lore for the physical rig. | Before you smash the "buy" button. |
| `roadmaps/` | Long-form design notes for current and future engines. | When you're planning a feature or trying to understand the vision. |
| `tutorials/scale_quantizer_flow.md` | Step-by-step quantizer lab that hooks UI experiments into `util::ScaleQuantizer`. | When you want to demo or test the scale quantizer story. |
| `tutorials/hal_poke_lab.md` | Mock HAL walkthrough for scripting audio pumps and GPIO edges without hardware. | When you need to write or explain tests that drive the panel via the simulator. |
| `tutorials/euclid_burst_pattern_lab.md` | Rhythm lab charting Euclid masks and Burst trigger clusters with reproducible logs. | When you’re teaching sequencer maths or validating new groove presets. |
| `calibration_guide.md` | Bench ritual for lining up hardware behaviour with firmware expectations, complete with log prompts. | Right after you assemble or service a unit and before demoing it. |
| `wiring_gallery.md` | Photo-friendly wiring reference that links each connection to the code relying on it. | While laying out harnesses or validating a builder’s work. |
| `troubleshooting_log.md` | Rolling ledger of failures, fixes, and the tests that proved them. | The moment a rig misbehaves or when closing out a calibration run. |

Suggested starting points in `roadmaps/`:
- [`roadmaps/granular.md`](roadmaps/granular.md)
- [`roadmaps/resonator.md`](roadmaps/resonator.md)
- [`roadmaps/native_golden.md`](roadmaps/native_golden.md)
- Pair these with runnable demos in [`tests/test_engine`](../tests/test_engine)
  when you want to see the math flexed in code.

## How we keep docs alive

- Update the matching doc the moment you change a code contract. Treat it like a
  lab notebook entry: date it, explain it, link to the change if possible.
- Capture experiments, even the weird ones. Future readers learn as much from
  the dead ends as the successes.
- Speak like a mentor, not a gatekeeper. Assume the reader is smart, curious,
  and maybe brand-new to embedded audio.

## Keeping CI honest

Our GitHub Actions workflow mirrors the quick-start loop and now covers the
 JUCE desktop world too:

1. `pio test -e native` keeps the algorithms honest.
2. `pio run -e teensy40` makes sure hardware builds stay tight while the env's `board_build.usbtype=USB_MIDI_SERIAL` pin keeps the USB persona locked to the synth-friendly MIDI+serial combo.
3. If `ENABLE_GOLDEN` is flipped on in a test run, CI publishes comparison data
   in `artifacts/` so we can review sound or log diffs without rerunning locally.
4. The JUCE desktop workflow builds a macOS universal (x86_64 + arm64) VST3 and
   standalone app (explicitly hits the VST3 format target so the bundle actually
   ships with a binary) on the macOS 14 runner to dodge the macOS 15 SDK’s
   missing CoreGraphics screen capture APIs, plus sanity builds on Linux and
   Windows that bolt GTK/WebKit/cURL pkg-config flags straight into the link to
   keep host dependencies in line. We also set `JUCE_VST3_CAN_REPLACE_VST2=OFF`
   so CI doesn’t chase the long-gone VST2 SDK when all we need is a VST3.
   Grab the runbook-style details in
   [`docs/ci_desktop_builds.md`](ci_desktop_builds.md).

You can stash local experiment renders in `out/` and quick `.wav` sketches in
either `out/` or `artifacts/`; both paths are ignored by git on purpose so
playful jams never clutter the history.

## License and credit

SeedBox ships with an MIT License. That permissive vibe matches our studio
ethos: remix freely, keep attribution, and feed discoveries back into the zine.
If you sample external schematics, credit the original source right in the doc —
we like our references punk and precise.

## Flag crib notes

Docs should mention the build flags whenever they matter. When in doubt, quote
the defaults straight from [`include/SeedBoxConfig.h`](../include/SeedBoxConfig.h):

- `SEEDBOX_HW` toggles hardware glue in code samples.
- `SEEDBOX_SIM` identifies native builds that keep IO mocked out.
- `QUIET_MODE` is the polite switch for silencing logs during demos or capture
  sessions (especially before saving `.wav` renders).
- `ENABLE_GOLDEN` turns on snapshot emission for regression guides; jot down
  where those files land so teammates can replay them.
- `SEEDBOX_DEBUG_CLOCK_SOURCE` unlocks the serial narration for transport
  decisions when you're debugging sync.
- `SEEDBOX_DEBUG_UI` is the playground for future overlays and class demos —
  document how you use it so others can riff.

Need receipts? `python scripts/describe_seedbox_config.py --format=markdown`
prints the authoritative table straight from the header so your doc never drifts.

Clarity keeps the jams inclusive. Write like you're inviting someone to sit in
on the next session.

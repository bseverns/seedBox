# SeedBox documentation staging ground

Welcome to the paper trail. If the firmware repo is the synth, this folder is
the zine you flip through on the bus ride to the gig. Every doc aims to teach,
not intimidate.

## What's inside

| File or folder | What you'll learn | Best time to read |
| --- | --- | --- |
| `index.md` / `WhySeedBox.md` / `CurrentState.md` / `ChooseYourPath.md` / `StabilityAndSupport.md` | The public-core identity, routing, status, and support layer. | First, especially if you want the repo’s shortest coherent overview. |
| `architecture/` | Service taxonomy, generated service graph, and JUCE real-time boundary audit. | Right after the first overview pass, especially if you're editing `AppState` or the host lane. |
| `BenchValidationMatrix.md` / `BenchReceiptTemplate.md` / `JUCESmokeChecklist.md` | Practical validation surfaces for hardware benches and desktop runtime checks. | Right before a demo, calibration pass, or manual smoke run. |
| `builder_bootstrap.md` | Environment setup, wiring diagrams, lab ideas. | When you're booting hardware or helping a friend get started. |
| `DOC_SINGLE_SOURCE.md` | Canonical ownership for overlapping docs so topics do not drift. | Before editing setup/build docs in multiple places. |
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
- [`roadmaps/platform_improvement_program.md`](roadmaps/platform_improvement_program.md)
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

CI mirrors the quick-start loop:

1. `pio test -e native`
2. `pio run -e teensy40`
3. optional golden fixture publishing when `ENABLE_GOLDEN=1`
4. desktop JUCE builds across macOS/Linux/Windows

Detailed runbooks live in:
- [`ci_desktop_builds.md`](ci_desktop_builds.md)
- [`juce_build.md`](juce_build.md)
- [`DOC_SINGLE_SOURCE.md`](DOC_SINGLE_SOURCE.md)

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
- `SEEDBOX_DEBUG_UI` flips on the JUCE debug HUD overlay (live state readouts)
  and reserves the hardware OLED overlay slot for later — document your reads
  so others can riff.

Need receipts? `python scripts/describe_seedbox_config.py --format=markdown`
prints the authoritative table straight from the header so your doc never drifts.

Clarity keeps the jams inclusive. Write like you're inviting someone to sit in
on the next session.

## External clock watchdog

When external clock is selected but no clock ticks arrive for ~2 seconds, the
app automatically falls back to the internal clock so audio resumes. The
watchdog is control-rate only and does not print from the audio thread.

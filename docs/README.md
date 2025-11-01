# SeedBox documentation staging ground

Welcome to the paper trail. If the firmware repo is the synth, this folder is
the zine you flip through on the bus ride to the gig. Every doc aims to teach,
not intimidate.

## What's inside

| File or folder | What you'll learn | Best time to read |
| --- | --- | --- |
| `builder_bootstrap.md` | Environment setup, wiring diagrams, lab ideas. | When you're booting hardware or helping a friend get started. |
| `assumptions.md` | Design bets, non-negotiables, and why quiet mode exists. | Before you ship a change that bends the vibe. |
| `ethics.md` | Privacy + data handling pact for labs and jams. | Any time you add IO, logging, or persistence. |
| `hardware_bill_of_materials.md` | Shopping list + sourcing lore for the physical rig. | Before you smash the "buy" button. |
| `roadmaps/` | Long-form design notes for current and future engines. | When you're planning a feature or trying to understand the vision. |
| _(future add-ons)_ | Calibration guides, wiring art, troubleshooting logs. | Whenever the community uncovers new stories worth sharing. |

Suggested starting points in `roadmaps/`:
- [`roadmaps/granular.md`](roadmaps/granular.md)
- [`roadmaps/resonator.md`](roadmaps/resonator.md)
- Pair these with runnable demos in [`test/test_engine`](../test/test_engine)
  when you want to see the math flexed in code.

## How we keep docs alive

- Update the matching doc the moment you change a code contract. Treat it like a
  lab notebook entry: date it, explain it, link to the change if possible.
- Capture experiments, even the weird ones. Future readers learn as much from
  the dead ends as the successes.
- Speak like a mentor, not a gatekeeper. Assume the reader is smart, curious,
  and maybe brand-new to embedded audio.

## Keeping CI honest

Our GitHub Actions workflow mirrors the quick-start loop:

1. `pio test -e native` keeps the algorithms honest.
2. `pio run -e teensy40` makes sure hardware builds stay tight while the env's `board_build.usbtype=USB_MIDI_SERIAL` pin keeps the USB persona locked to the synth-friendly MIDI+serial combo.
3. If `ENABLE_GOLDEN` is flipped on in a test run, CI publishes comparison data
   in `artifacts/` so we can review sound or log diffs without rerunning locally.

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

# Assumption ledger

Every project hides a few bets. Here's the running list so we can revisit them
intentionally instead of tripping over them mid-jam.

## Musical system

- Seeds default to four voices because that's enough to teach density without
  overwhelming newcomers.
- Engines stay deterministic unless `QUIET_MODE` is disabled. We value repeatable
  labs over chaotic performances for now.
- Tempo is 120 BPM out of the box. Classroom demos can change it, but tests rely
  on the default.

## Hardware

- Teensy 4.0 + SGTL5000 audio shield remains the reference platform.
- Rotary encoders use internal pull-ups; hardware docs assume you keep them.
- USB MIDI is the primary transport. DIN is optional but supported.

## Tooling

- PlatformIO is the build system. We do not maintain Arduino IDE sketches.
- Native builds should compile without Teensy headers present.
- Golden audio fixtures will be 16-bit PCM WAVs stored out-of-tree (TODO: add
  generation scripts).

## Social contract

- Documentation is part of the deliverable. Every new feature requires at least
  a README blurb or doc update.
- No binary assets in git. If you need to reference sound, leave a `TODO: listen
  here` marker with the intended path.
- Contributors respond to review feedback within a week or mark the PR as draft.

Append to this ledger whenever you catch an implicit belief. Clarity keeps the
music weird *and* reliable.

# Ethics + privacy stance

SeedBox is playful, but we still guard player privacy and agency. Here's what
that means today.

## Quiet by default

`QUIET_MODE` keeps the firmware from phoning home: no persistent storage writes,
no surprise MIDI clock spam, and deterministic seeds. Examples and native tests
leave it enabled so workshops stay self-contained. Turning it off (`-D QUIET_MODE=0`)
should be a conscious choice documented in your PR or lab notes.

## Data collection

- We collect **no** telemetry.
- Serial logs are opt-in for debugging only. Never ship personal data in logs.
- TODO: publish guidance on anonymizing shared audio clips once fixtures exist.

## Accessibility + inclusion

- Document every interaction in clear language. Assume someone is reading after a
  long studio night.
- Keep hardware instructions screen-reader friendly: tables, alt-text, and
  captions matter.
- Encourage contributions from people experimenting with adaptive controllers.

## Responsible experimentation

- Share new audio algorithms with context. Explain the musical intent and any
  safety considerations (loudness, unexpected feedback paths).
- When borrowing ideas from other projects, credit them in docs and link to their
  licenses.
- Flag any features that could capture external audio. Make sure performers know
  when they're recording.

This ethos should evolve. If you spot gaps, open an issue or PR with proposed
updates so the community can respond together.

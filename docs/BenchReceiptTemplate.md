# Bench Receipt Template

Use this when a physical SeedBox unit has actually been on the bench. The goal
is not polished prose. The goal is a dated, replayable receipt that ties a unit,
a build, a wiring state, and a validation pass together.

Paste a filled-in copy into:

- [`docs/troubleshooting_log.md`](troubleshooting_log.md)
- a hardware probe note under [`docs/hardware/`](hardware)
- a PR description when the hardware result matters to the change

## Copy/paste template

```md
### Bench receipt — YYYY-MM-DD

- Unit / build label:
- Git revision:
- Firmware command:
- Builder / tester:
- Hardware stack:
  - Teensy:
  - Audio shield:
  - OLED:
  - Input controls:
  - MIDI wiring:
- Substitutions or quirks:

#### What was being validated

- [ ] Boot and audio wake
- [ ] OLED / UI state
- [ ] Encoders and buttons
- [ ] USB MIDI
- [ ] TRS MIDI / external clock
- [ ] Granular hardware fan-out
- [ ] End-to-end calibration pass

#### Repo-side proof run first

- [ ] `pio run -e teensy40`
- [ ] `pio test -e native`
- [ ] `pio test -e teensy40 --filter test_hardware` (if relevant)

#### Bench results

- Boot / audio:
- OLED / UI:
- Encoders / buttons:
- Clock / transport:
- Granular probes:
- Other observations:

#### Receipts

- Serial log:
- Photos:
- Scope / analyzer capture:
- Audio capture:

#### Outcome

- Status: pass / pass-with-caveats / fail
- Follow-up needed:
- Linked troubleshooting entry:
```

## Minimum useful version

If time is short, do not skip the receipt entirely. Record at least:

- date
- unit/build label
- git revision
- exact firmware command
- what passed
- what failed
- where the logs/photos live

## Why this exists

The simulator and golden pipeline give SeedBox strong repo-side proof. Physical
units still need local receipts so "worked on my desk" becomes something the
next builder can actually replay.

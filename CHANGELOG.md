# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Added a hardware board abstraction with input event plumbing and controller
  tests so the firmware targets Teensy hardware and the simulator with the same
  mental model.
- Introduced clock provider modules, engine router metadata, Euclid/Burst
  generators, and swing/tap/prime gestures to flesh out the sequencing
  workflow.
- Built preset storage backends with EEPROM compression plus a config narrator
  CI script so configuration/state handling stays predictable.
- Landed scripted walkthrough/timing goldens and a native entry point to widen
  coverage for CI and local repros.

### Changed
- Swapped to the SparkFun Qwiic OLED, adopted Type-A TRS MIDI with a Serial7
  driver, normalized MN42 channel handling, and mapped seed/density controls to
  hardware so the panel matches firmware expectations.
- Upgraded audio clocking by raising simulator and hardware sample rates to
  48 kHz, centralizing audio memory budgeting, cascading resonator mixers, and
  tightening quiet-mode/transport routing.
- Hardened seed workflows and UI hints by tightening macros, quantize state
  transitions, and swing pop-over behavior.

### Fixed
- Patched a stack of build issues: MidiRouter backend creation/declarations,
  CLI warnings, duplicate helpers, AppState members/braces, storage state
  transitions, ArduinoJson document construction, and PlatformIO guard usage.
- Resolved native build/test instabilities including double-main definitions,
  guard semantics, debug clock toggles, and quiet-mode alignment with the
  golden harness.
- Stabilized engine scheduling by fixing external clock handoffs, pattern
  scheduler sample math, MN42 handshake resets, granular SD fallback, and
  transport jitter.

### Documentation
- Sketched the next calibration + release checklist so future-us has a
  dog-eared map instead of a hazy memory.
- Expanded README, builder, and roadmap docs with hardware lists, examples, and
  teaching notes; refreshed badges and clarified Teensy model targeting.
- Annotated tests and Teensy sources with teaching-friendly comment blocks for
  density, external MIDI priority, MN42 CC intent, resonator voice pools, and
  simulator expectations.

### Tests
- Added scripted walkthrough regressions, timing goldens, and expanded native
  plus hardware CI (including config narration) to keep the harness
  authoritative.
- Broadened quiet-mode, swing logging, and simulator audio regression coverage
  to track new behaviors.

### Wishlist
- Audio fixture recordings and reference spectra are still on the wishlist;
  they'll land once we lock in the signal chain.
## [0.1.0] - 2024-05-28

### Added
- First hardening pass on build docs and test scaffolding, making it easier for
  newcomers to spin up the lab without frying their patience.
- Release playbook documentation so we stop improvising tagging rituals at 2am.

### Security
- Audited PlatformIO dependencies and tightened native test defaults so the
  sandbox stays hermetic.

[Unreleased]: https://github.com/example/seedBox/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/example/seedBox/releases/tag/v0.1.0

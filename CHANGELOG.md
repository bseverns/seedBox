# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

- TODO: Capture golden audio fixtures and host micro clips for the examples.

## [0.1.0] - 2025-10-11

### Added

- MIT licensing for code, CC-BY-4.0 for docs, and governance docs (Code of
  Conduct, contributing guide, security policy).
- PlatformIO environments pinned for native and Teensy 4.0 builds with matching
  toolchain notes.
- HAL seam for audio/IO plus documentation of timing guarantees and mocking
  strategy.
- Deterministic "golden audio" test harness stub gated behind `ENABLE_GOLDEN`.
- CI workflow running native tests and Teensy smoke builds without artifacts.
- `QUIET_MODE` flag and quiet-friendly playable examples (`sprout`, `reseed`,
  `headless`) that double as lab notes.
- Interop constants and guide for the MN42 ecosystem.
- Documentation pass covering assumptions, ethics stance, builder primer,
  toolchain pinning, release steps, and TODO audio clip markers.

### Changed

- README polished with badges, expanded orientation, diagrams, and crosslinks to
  the new examples/tests.
- `.gitignore` updated for audio fixtures and future artifact directories.

[Unreleased]: https://github.com/bseverns/seedbox/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/bseverns/seedbox/releases/tag/v0.1.0

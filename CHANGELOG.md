# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

- TODO: Score new golden audio fixtures and publish tiny WAV previews.
- TODO: Expand MN42 handshake coverage with real integration captures.
- Fixed: Pin the Encoder library via Git tag so CI can fetch Teensy deps reliably.
- Fixed: Pin Teensy builds to the upstream PaulStoffregen library IDs so CI
  stops choking on the Audio stack.

## [0.1.0] - 2025-02-14

### Added
- MIT license for code and CC-BY-4.0 for docs.
- Contributor Covenant, contributing guide, security policy, and release notes.
- Pinned PlatformIO environments for native and Teensy 4.0 builds.
- HAL seam for audio and IO timing audits with documentation.
- Deterministic golden-audio harness stubs gated behind `ENABLE_GOLDEN`.
- Minimal native examples (`01_sprout`, `02_reseed`, `03_headless`) using `QUIET_MODE`.
- MN42 interop constants and handshake guide.
- Assumption ledger and ethics notes, including the default `QUIET_MODE` stance.
- CI workflow covering native tests and Teensy smoke builds.
- Expanded README with diagrams, badges, and explicit TODO markers for audio captures.

### Changed
- README tone polished to highlight the studio-notebook ethos and new docs.
- `.gitignore` updated for future audio and artifact outputs.

### Security
- Documented responsible disclosure workflow and safe harbor policy.

[Unreleased]: https://github.com/bseverns/seedbox/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/bseverns/seedbox/releases/tag/v0.1.0

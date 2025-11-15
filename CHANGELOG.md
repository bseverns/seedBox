# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Dropped a full native golden harness pipeline (hash compute script, fixtures,
  orchestrated harness entry point) so we can snapshot-regress the audio engine
  without touching hardware.
- Delivered a scale quantizer tutorial: new docs, PlatformIO example project,
  tests, and a narrated walkthrough that doubles as a regression suite.
- Added CLI toggles and runtime hooks to the Sprout simulator so folks can
  rehearse reseed/transport tricks from the keyboard while we polish the panel.
- Annotated the seed genome struct, granular/reseed roadmap coverage, and
  golden-harness playbooks with lab-style READMEs to keep the intent
  teachable.

### Changed
- Radically broadened the native golden harness gallery (new fixtures, longer
  takes, console/mixer probes) and refreshed expected hashes to match the new
  capture format.
- Reworked simulator walkthroughs and docs to highlight CLI flows, reseed
  timing, and external clock priming now that the harness guards those paths.

### Deprecated
- No deprecations this cycle—keep riding the current env lineup.

### Fixed
- Cleared a pile of hash drift, golden refresh failures, and reseed log shuffle
  nondeterminism uncovered by the new harness workflow.
- Smoothed out native test build quirks (double mains, guard semantics, debug
  clock toggles) uncovered while wiring the compute script into CI.

### Documentation
- Punched up READMEs across docs/, examples/, and tests/ with intent-first
  narratives, wiring diagrams, and "what to listen for" callouts.
- Documented granular/reseed coverage, the seed genome, and the golden harness
  roadmap so contributors can trace signal flow before touching C++.
- Logged how we expect folks to narrate changelog updates so the future crew
  sees the same punk-leaning, lab-notebook vibe we're aiming for when they add
  their own riffs.

### Tests
- Assembled scripted walkthrough regressions, timing goldens, and expanded
  native plus hardware CI (including config narration) to keep the harness
  authoritative.
- Broadened quiet-mode, swing logging, and simulator audio regression coverage
  to track new behaviors.
- Regenerated golden hashes and fixtures after every harness tweak so CI stays
  in lockstep with the captured audio truth tables.

### Wishlist
- Nothing on deck this cycle—the wishlist is clear while we let the harness
  bake. Future experiments will land once the crew files fresh tickets.
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

# SeedBox repo health audit — March 2026

This audit is a concise framing pass for the repo as it exists today. It stays
grounded in the current README, onboarding/bootstrap docs, build docs, tests,
examples, hardware notes, and `platformio.ini`.

## What SeedBox is

SeedBox is a cross-surface instrument system organized around one shared core:

- a Teensy 4.0 firmware target in [`platformio.ini`](../platformio.ini)
- a laptop-native simulator and test harness in [`tests/README.md`](../tests/README.md)
- a JUCE standalone/VST3 lane in [`docs/juce_build.md`](juce_build.md) and
  [`src/juce/README.md`](../src/juce/README.md)
- a deterministic golden-render pipeline in
  [`docs/roadmaps/native_golden.md`](roadmaps/native_golden.md)
- a real hardware build path in
  [`docs/hardware_bill_of_materials.md`](hardware_bill_of_materials.md) and
  [`docs/builder_bootstrap.md`](builder_bootstrap.md)
- a teaching and trust layer expressed through docs, tests, ethics, and signed
  artifacts

The strongest through-line is the shared seed/app/hal architecture: the repo
repeatedly presents hardware, simulator, and JUCE as different bodies for the
same instrument logic rather than unrelated side projects.

## What is already unusually strong

- The repo has a clear authored voice. It already reads like a lab notebook,
  zine, and teaching environment rather than anonymous infrastructure.
- The proof culture is unusually public. Native tests, golden renders,
  manifests, control logs, offline refresh tooling, and signing docs make
  claims inspectable.
- Cross-platform intent is explicit. `platformio.ini`, JUCE/CMake docs, and the
  shared source tree all point at one codebase spanning firmware, sim, and
  desktop.
- Hardware is treated seriously. There is a BOM, pin map, calibration guidance,
  wiring support, and hardware-only probing instead of vague "future hardware"
  language.
- The documentation is pedagogical, not just operational. `src/README.md`,
  `tests/README.md`, `scripts/README.md`, and example READMEs all teach how the
  system works.
- Trust surfaces exist already. Ethics and artifact-signing docs make the repo
  feel accountable, not just clever.

## What currently makes it feel fragmented or under-consolidated

- The repo has many strong surfaces but no small canonical set for outsiders.
  Identity, onboarding, build truth, stability, and proof are spread across
  README, onboarding, bootstrap, roadmaps, tests, and build docs.
- There is no single "current truth" page that says what is solid, what is
  frontier, and what still needs bench validation.
- Stable, experimental, and future-facing material live close together without a
  consistent framing layer. Honest caveats exist, but they are distributed.
- A visitor can understand individual lanes, yet still miss the authored system
  that connects firmware, simulator, JUCE, hardware, docs, and tests.
- The public front door still feels lab-first rather than ecosystem-first. The
  substance is here; the consolidation is not.

## What is missing for key audiences

### Newcomer

- One chooser page that routes by intent instead of folder structure
- One identity page that explains what SeedBox is without assuming repo fluency
- One status page that distinguishes "explore now" from "watch this space"

### Musician

- A simple path that starts with "hear it now" and explains how simulator,
  examples, and JUCE differ
- A practical trust page describing what parity to expect between laptop and
  hardware

### Builder

- A canonical summary of which hardware claims are bench-proven versus merely
  documented
- A clearer connection between bootstrap instructions and current support
  boundaries

### Collaborator

- A concise public map of canonical docs
- A short explanation of what "supported" means in this repo
- A better bridge from proof artifacts to public understanding

## What makes it feel less publicly complete than a mature sibling project

- The repo has internal rigor but lacks a small public-core doc set.
- Identity is implied more often than stated.
- Current status is inferable, but not declared.
- Proof material is powerful, but it reads like internal engineering evidence
  more than a public gallery surface.
- There is not yet a single page that says, plainly, "this is one instrument
  system, here is how its bodies relate, here is where to start, and here is
  what is proven."

## Canonical entry points to establish

These should become the public core:

- [`README.md`](../README.md): front door and shortest explanation
- [`docs/ChooseYourPath.md`](ChooseYourPath.md): intent-based router
- [`docs/WhySeedBox.md`](WhySeedBox.md): identity and system framing
- [`docs/CurrentState.md`](CurrentState.md): current truth surface
- [`docs/StabilityAndSupport.md`](StabilityAndSupport.md): practical trust model
- [`docs/SeedGallery.md`](SeedGallery.md): public proof/listening surface

These remain important specialist docs behind the public core:

- onboarding: [`docs/onboarding/newcomer_map.md`](onboarding/newcomer_map.md)
- build/bootstrap: [`docs/builder_bootstrap.md`](builder_bootstrap.md)
- JUCE: [`docs/juce_build.md`](juce_build.md)
- tests/goldens: [`tests/README.md`](../tests/README.md) and
  [`docs/roadmaps/native_golden.md`](roadmaps/native_golden.md)
- hardware: [`docs/hardware_bill_of_materials.md`](hardware_bill_of_materials.md)

## Recommended next move

Do not flatten the repo into generic product copy. Instead:

- keep the lab/zine/teaching voice
- add a clearer public skeleton
- make stability boundaries explicit
- present tests, docs, and goldens as part of the instrument body
- keep TODO markers where evidence is still incomplete

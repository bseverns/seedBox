# Current State

This page is the repo’s practical "current truth" surface. It is not a promise
of finish. It is a concise reading of what the repository currently supports,
documents, and proves.

Status labels used here:

- `Explore now`: there is enough repo evidence to make this a sensible starting
  point
- `Experimental / frontier`: real and worth exploring, but still evolving or
  needing more validation
- `Documented but evolving`: there is meaningful guidance already, but the
  surface is still consolidating

## System-wide view

### Explore now

- Native simulator and test path
- Deterministic golden-render pipeline
- Core firmware build/test flow
- Teaching docs and examples as learning surfaces

### Experimental / frontier

- JUCE desktop lane as a broader public-facing path
- Hardware parity at the level of bench-proven repeatability across different
  builds and setups

### Documented but evolving

- Hardware bring-up guidance
- Example/tutorial coverage as a complete public curriculum

## Firmware

**Status:** `Explore now`

**What the repo clearly shows**

- `platformio.ini` defines a `teensy40` environment and the shared build flags
  that keep firmware aligned with the rest of the system.
- Builder and hardware docs describe the Teensy 4.0 path, pin map, and bench
  rituals.
- Tests and source docs treat firmware as the central body, not an afterthought.

**What to keep in mind**

- Firmware buildability and architecture are clearly first-class.
- Real hardware behavior still depends on the exact bench setup, wiring,
  peripherals, and calibration state.

**Start here**

- [`docs/builder_bootstrap.md`](builder_bootstrap.md)
- [`docs/hardware_bill_of_materials.md`](hardware_bill_of_materials.md)

## Native simulator

**Status:** `Explore now`

**What the repo clearly shows**

- The README and builder bootstrap both present the native path as the first
  low-friction route.
- `platformio.ini` defines `native` and `native_golden` environments.
- `tests/README.md` presents native testing as the project heartbeat.
- Examples such as `examples/01_sprout` and `examples/03_headless` reinforce the
  laptop-first exploration path.

**What to keep in mind**

- This is currently the clearest newcomer path.
- Simulator confidence is strongest around shared logic, deterministic output,
  and testable behaviors, not around proving every hardware nuance.

**Start here**

- [`docs/ChooseYourPath.md`](ChooseYourPath.md)
- [`tests/README.md`](../tests/README.md)
- [`examples/01_sprout/README.md`](../examples/01_sprout/README.md)

## JUCE builds

**Status:** `Experimental / frontier`

**What the repo clearly shows**

- There is a real JUCE/CMake lane with shared-core targets for a standalone app
  and a VST3 build.
- The repo contains detailed JUCE build docs, CI desktop build docs, and a
  JUCE-specific source README.
- The desktop lane is intentionally framed as a host/sim twin of the shared
  instrument logic.

**What to keep in mind**

- This lane is real, documented, and meaningful.
- It is not yet the simplest or most canonical first contact compared with the
  native simulator.
- Host integration, OS packaging, and UX polish should still be treated as
  evolving surfaces.
- A dated local macOS receipt now exists in
  [`docs/JUCESmokeChecklist.md`](JUCESmokeChecklist.md), but it covers
  rebuild + standalone launch, a REAPER host-start/cache pass, and a local
  VST3 artifact-audio probe that rendered and played a non-silent test tone.
  A full in-DAW monitoring/session-behavior pass is still a separate receipt.

**Start here**

- [`docs/juce_build.md`](juce_build.md)
- [`docs/ci_desktop_builds.md`](ci_desktop_builds.md)
- [`docs/JUCESmokeChecklist.md`](JUCESmokeChecklist.md)

## Hardware path

**Status:** `Documented but evolving`

**What the repo clearly shows**

- The repo contains a serious hardware build path: BOM, wiring notes, bootstrap
  guidance, calibration docs, and hardware probes.
- The project is explicit about the Teensy 4.0, SGTL5000 audio path, OLED, MIDI
  wiring, and panel controls.

**What to keep in mind**

- This is a real builder path, not a fictional roadmap bullet.
- It still requires bench validation, assembly care, and unit-specific sanity
  checks.
- The repo is honest about hardware as a lab surface rather than a shrink-wrapped
  product.

**Start here**

- [`docs/hardware_bill_of_materials.md`](hardware_bill_of_materials.md)
- [`docs/builder_bootstrap.md`](builder_bootstrap.md)
- [`docs/calibration_guide.md`](calibration_guide.md)
- [`docs/BenchValidationMatrix.md`](BenchValidationMatrix.md)

## Deterministic golden pipeline

**Status:** `Explore now`

**What the repo clearly shows**

- The repo has a dedicated `native_golden` environment, manifest, fixture
  catalog, hash tooling, offline fallback tooling, and documentation.
- Golden renders include both audio fixtures and control logs, which makes the
  proof surface legible to both ears and eyes.
- This is one of the strongest current public assets in the repository.

**What to keep in mind**

- Goldens prove deterministic native outputs and control transcripts.
- They do not, by themselves, prove complete hardware parity on every bench
  setup.

**Start here**

- [`docs/SeedGallery.md`](SeedGallery.md)
- [`docs/roadmaps/native_golden.md`](roadmaps/native_golden.md)
- [`tests/native_golden/README.md`](../tests/native_golden/README.md)

## Examples and tutorials

**Status:** `Documented but evolving`

**What the repo clearly shows**

- There is already a teaching path through examples, tutorials, folder READMEs,
  and onboarding docs.
- `examples/01_sprout` is a strong first small encounter.
- Tutorials cover concrete subjects such as live input, scale quantizing, HAL
  poking, and Euclid/Burst pattern work.

**What to keep in mind**

- The teaching material is generous and valuable.
- Coverage is still uneven across all system layers, especially if a visitor is
  looking for a single guided curriculum from "hear it" to "build it."

**Start here**

- [`docs/onboarding/newcomer_map.md`](onboarding/newcomer_map.md)
- [`examples/01_sprout/README.md`](../examples/01_sprout/README.md)
- [`docs/tutorials/`](tutorials)

## Known gaps in the public surface

- No single page previously combined identity, routing, status, support, and
  proof.
- Hardware validation expectations were inferable, but not gathered into one
  practical trust document.
- The relationship between simulator, JUCE, and hardware has been present, but
  not yet stated in one concise outsider-facing explanation.

## TODOs

- TODO: add real dated hardware bench entries using the receipt template once
  repeated bench runs are documented in one place.
- TODO: add more dated JUCE smoke entries across actual hosts/DAWs so the
  desktop lane has runtime receipts beyond the first local macOS launch pass.

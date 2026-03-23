# SeedBox — lab for sound + code curious humans

SeedBox is a seed-driven instrument system with several bodies: a Teensy 4.0
firmware build, a laptop-native simulator, a JUCE standalone/VST3 lane, a
deterministic golden-render harness, and a hardware/documentation environment
built for teaching as much as building.

Think of this repo as half studio notebook, half teaching zine. The code makes
noise, the tests keep receipts, and the docs explain how the same instrument
logic moves across hardware, simulator, desktop, and bench work.

## What SeedBox is

- a Teensy 4.0 firmware project
- a laptop-native simulator and test path
- a JUCE standalone / VST3 lane built from the same core
- a deterministic golden-render environment for regression proof
- a hardware build path with BOM, wiring, and calibration notes
- a trust-conscious documentation layer with ethics and artifact-signing docs

## Start here

- **Choose a route:** [`docs/ChooseYourPath.md`](docs/ChooseYourPath.md)
- **Understand the system:** [`docs/WhySeedBox.md`](docs/WhySeedBox.md)
- **Check the current truth:** [`docs/CurrentState.md`](docs/CurrentState.md)

## Make noise fast

### Laptop-native (no hardware)

1. Install PlatformIO once: `pip install -U platformio`
2. Clone and enter: `git clone https://github.com/bseverns/seedBox.git && cd seedBox`
3. Run the starter bundle: `./scripts/starter_bundle.sh`
4. Start with [`examples/01_sprout/README.md`](examples/01_sprout/README.md)
5. Optional: use the pinned container in [`containers/native-dev/README.md`](containers/native-dev/README.md)

This is the clearest first path if you want to hear SeedBox and learn the shared
logic without soldering first.

### Teensy 4.0 hardware

1. Read [`docs/CurrentState.md`](docs/CurrentState.md) and
   [`docs/StabilityAndSupport.md`](docs/StabilityAndSupport.md)
2. Wire from [`docs/hardware_bill_of_materials.md`](docs/hardware_bill_of_materials.md)
   and [`docs/builder_bootstrap.md`](docs/builder_bootstrap.md)
3. Build the firmware: `pio run -e teensy40`
4. Bench-check with [`docs/calibration_guide.md`](docs/calibration_guide.md)

### JUCE / desktop lane

- Build the standalone app or VST3 from [`docs/juce_build.md`](docs/juce_build.md)
- Use [`docs/ci_desktop_builds.md`](docs/ci_desktop_builds.md) for platform
  expectations
- Treat JUCE as the desktop body of the same system, not a separate repo story

## One system, several surfaces

- **Firmware:** the physical instrument path on Teensy 4.0
- **Native simulator:** fastest way to explore, test, and render deterministically
- **JUCE:** desktop-host body for standalone and DAW contexts
- **Seed genome + shared core:** the thread that keeps behavior legible across
  targets
- **Docs and tests:** part of the instrument body, not just supporting paperwork

## Where to read next

- **Orientation zine:** [`docs/onboarding/newcomer_map.md`](docs/onboarding/newcomer_map.md)
- **Builder primer:** [`docs/builder_bootstrap.md`](docs/builder_bootstrap.md)
- **Source tour:** [`src/README.md`](src/README.md)
- **Test harness:** [`tests/README.md`](tests/README.md)
- **Stability boundary:** [`docs/StabilityAndSupport.md`](docs/StabilityAndSupport.md)
- **Proof/listening surface:** [`docs/SeedGallery.md`](docs/SeedGallery.md)

## Trust, tests, and receipts

- **Deterministic audio:** golden fixtures + manifests live in
  [`tests/native_golden/`](tests/native_golden) with deeper notes in
  [`docs/roadmaps/native_golden.md`](docs/roadmaps/native_golden.md)
- **Signed bundles:** CI artifact verification steps live in
  [`docs/security/artifact_signing.md`](docs/security/artifact_signing.md)
- **Ethics:** privacy and data-handling expectations live in
  [`docs/ethics.md`](docs/ethics.md)

Welcome to the lab. Keep it loud, keep it intentional, and narrate every
experiment so the next traveler can build on it.

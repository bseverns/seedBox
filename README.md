# SeedBox — punky seed lab for sound + code curious humans

SeedBox is a tiny music lab wrapped in C++. It runs as Teensy 4.0 firmware **and** as a laptop simulator, so you can sketch rhythms, mangle grains, or teach DSP without solder anxiety. Think of this repo as half studio notebook, half teaching zine: the code makes noise, and the docs explain why.

## Who should be here
- **New programmers:** follow the prompts, run the tests, and learn how a seed genome becomes audio.
- **Musicians:** plug in a Teensy, reseed a preset bank, and hear the same grooves on hardware and the sim.
- **Punks + tinkerers:** break the DSP, rewrite a scheduler, or script headless reseeds; the repo keeps receipts.

## Make noise fast
### Laptop-native (no hardware)
1. Install PlatformIO once: `pip install -U platformio`.
2. Clone and enter: `git clone https://github.com/bseverns/seedBox.git && cd seedBox`.
3. Grab deps: `pio pkg install`.
4. Run the fast tests: `pio test -e native`.
5. Fire up the simulator examples (headless reseeds, ASCII UI mirrors) in `examples/` to hear the engines without flashing a board.

### Teensy 4.0 hardware
1. Wire it up with the guides in [`docs/hardware/`](docs/hardware) and the BOM in [`docs/hardware_bill_of_materials.md`](docs/hardware_bill_of_materials.md).
2. Build the firmware: `pio run -e teensy40`.
3. Reseed from the front panel or MIDI and compare against the simulator to prove parity.

### Already hacking code?
- Crack open [`docs/builder_bootstrap.md`](docs/builder_bootstrap.md) and [`src/README.md`](src/README.md) for the architecture tour.
- Run `pio test -e native_golden` when you touch audio paths so reviewers can diff deterministic renders.
- Need a DAW lane? Follow [`docs/juce_build.md`](docs/juce_build.md) to build the standalone app or VST3.

## Where to read next
- **Orientation zine:** [`docs/onboarding/newcomer_map.md`](docs/onboarding/newcomer_map.md) maps the repo, shows the engine cabinet, and links the first doc in each folder.
- **Seed primes, proved:** [`docs/seeds/prime_modes.md`](docs/seeds/prime_modes.md) walks through LFSR, tap-tempo, and preset primes plus the tests that lock them down.
- **Tutorials + galleries:** Peek at [`docs/tutorials/`](docs/tutorials) for live input, quantizer, Euclid/Burst rhythm labs, and HAL poking sessions, or at [`docs/ui_ascii_gallery.md`](docs/ui_ascii_gallery.md) for UI snapshots.
- **Examples that make noise:** [`examples/`](examples) double as lesson plans; start with [`examples/01_sprout/README.md`](examples/01_sprout/README.md).
- **Hardware helpers:** Wiring, calibration, and panel cheat sheets live under [`docs/hardware/`](docs/hardware) and [`docs/panel_cheat_sheet.md`](docs/panel_cheat_sheet.md).
- **Ethos + safety:** See [`CODE_OF_CONDUCT.md`](CODE_OF_CONDUCT.md) and [`docs/ethics.md`](docs/ethics.md).

## Trust, tests, and receipts
- **Deterministic audio:** Golden fixtures + manifests live alongside the tests in [`tests/native_golden/`](tests/native_golden) with deeper notes in [`docs/roadmaps/native_golden.md`](docs/roadmaps/native_golden.md). Run `pio test -e native_golden` to regenerate proofs when you change DSP.
- **Signed bundles:** CI publishes artifacts with detached GPG signatures; verification steps live in [`docs/security/artifact_signing.md`](docs/security/artifact_signing.md).
- **Cross-check builds:** Desktop CI recipes and dependencies stay documented in [`docs/ci_desktop_builds.md`](docs/ci_desktop_builds.md).

Welcome to the lab. Keep it loud, keep it intentional, and narrate every experiment so the next traveler can build on it.

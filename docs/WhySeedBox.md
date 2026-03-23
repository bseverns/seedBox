# Why SeedBox

SeedBox is not just a firmware repo and not just a simulator. It is an
instrument system with several bodies that all point back to the same musical
logic.

At the center is a shared seed-driven core: the same seed genome, scheduler,
app state, UI snapshot logic, and engine code are used to drive a Teensy 4.0
build, a laptop-native simulator, and a JUCE desktop lane. The repo’s docs,
tests, and deterministic renders are part of that system too. They do not sit
outside the instrument; they are how the instrument explains itself, proves
itself, and teaches itself.

## One instrument, several bodies

### Hardware

The physical SeedBox path is the Teensy 4.0 plus audio/UI/control stack
documented in the builder and hardware guides. This is where the project meets
knobs, wiring, MIDI, the OLED, and bench reality.

### Native simulator

The native path is the fastest way to meet SeedBox. It lets you run shared
logic on a laptop, exercise the tests, render deterministic output, and learn
the system without committing to hardware first.

### JUCE standalone / VST3 lane

The JUCE path gives SeedBox a host-facing desktop body. It is not presented
here as a separate product. It is a desktop expression of the same instrument
logic, useful for DAW integration, desktop listening, and architecture
continuity.

### Seed genome logic

The seed structure is the musical through-line. In this repo, reseeding is not
just preset switching. It is a way of carrying a compact musical identity
through firmware, simulator, UI snapshots, tests, examples, and golden renders.

### Docs and tests

SeedBox treats documentation and tests as part of the public instrument body.
The docs teach the shape of the system. The tests and golden renders provide
receipts. Together they make the project more legible, more teachable, and more
trustworthy.

## Why this shape matters

A lot of audio repositories split into isolated islands: hardware here, plugin
there, docs somewhere else, and tests as private maintenance work. SeedBox is
stronger when those pieces feel authored as one thing.

That is the point of the project’s dual and triple lives:

- prototype and learn on a laptop
- move toward hardware without rewriting the mental model
- carry the same musical logic into desktop-host contexts
- make experiments reproducible enough to discuss, test, and teach

## What makes SeedBox different

- It treats pedagogy as a first-class feature.
- It exposes receipts instead of hiding behind vague claims.
- It keeps experimental work visible, but tries to label it honestly.
- It values trust surfaces such as ethics notes, deterministic fixtures, and
  signed artifacts.

## What SeedBox is not

- not a generic consumer product page
- not a promise that every path is equally mature
- not a request to ignore experimental edges

It is a public lab instrument with enough rigor to be learned, heard, tested,
built, and extended in the open.

## Read next

- [Choose your path](ChooseYourPath.md)
- [Current state](CurrentState.md)
- [Stability and support](StabilityAndSupport.md)
- [Seed gallery](SeedGallery.md)

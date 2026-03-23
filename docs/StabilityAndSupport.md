# Stability And Support

This is a practical trust document for SeedBox as it exists in this repo.
It explains what is well-proven, what is deterministic, what still needs bench
validation, and what "supported" means here.

## What is well-proven today

- The shared core architecture is deliberate and repeatedly documented across
  firmware, native, and JUCE surfaces.
- The native simulator/test lane is a strong proof surface for shared logic,
  scheduler behavior, UI snapshots, and deterministic render paths.
- The golden pipeline is one of the repo’s clearest trust assets: fixtures,
  manifests, hashes, and control logs give reviewable receipts.
- The build and hardware documentation is substantial enough to make SeedBox
  legible as a real builder ecosystem, not just an internal experiment.

## What is deterministic and test-backed

These are the strongest "receipts" in the repository:

- `pio test -e native`
- `pio test -e native_golden`
- [`tests/native_golden/golden.json`](../tests/native_golden/golden.json)
- [`docs/roadmaps/native_golden.md`](roadmaps/native_golden.md)
- the app/pattern/engine test suites described in [`tests/README.md`](../tests/README.md)

In practical terms, this means the repo has strong evidence for:

- seed-driven shared logic
- deterministic scheduler/pattern behavior
- repeatable native renders and control ledgers
- regression review based on artifacts, not memory

## What still requires bench validation

Native proofs are not the same thing as full hardware proof. Bench validation is
still required for:

- exact hardware wiring outcomes
- OLED, encoder, button, and MIDI physical behavior on a given unit
- audio-shield noise/performance on a given bench supply and enclosure layout
- timing and transport behavior when real external devices are involved
- real-world parity checks between a physical build and the simulator/JUCE path

If you are building or demonstrating a physical unit, treat the following docs
as mandatory companions, not optional reading:

- [`docs/builder_bootstrap.md`](builder_bootstrap.md)
- [`docs/hardware_bill_of_materials.md`](hardware_bill_of_materials.md)
- [`docs/calibration_guide.md`](calibration_guide.md)
- [`docs/BenchValidationMatrix.md`](BenchValidationMatrix.md)
- [`docs/BenchReceiptTemplate.md`](BenchReceiptTemplate.md)

## What "supported" means here

In this repo, "supported" means some combination of the following is true:

- there is an intentional code path in the repository
- there is documentation for how to use or build it
- there are tests, goldens, or explicit validation rituals around it
- the maintainers are trying to keep the story coherent across surfaces

It does not mean:

- guaranteed plug-and-play behavior on every machine or bench
- complete polish across every UI and build path
- hardware QA equivalent to a commercial manufacturing pipeline
- perfect parity between all targets under all conditions

## Simulator vs hardware parity

The right mental model is "shared musical logic, different physical realities."

What the simulator is good at proving:

- the seed/app/scheduler logic
- many UI state transitions
- deterministic output and regression coverage
- fast iteration without hardware friction

What hardware is still needed to prove:

- physical IO correctness
- analog and codec behavior
- cable, power, and peripheral interactions
- ergonomic truth at the panel

The simulator is not fake. It is one of the instrument’s main proof surfaces.
But it should not be mistaken for a full substitute for bench validation.

## Where experimental edges live

The main experimental edges currently appear around:

- the broader JUCE desktop lane as a public-facing experience
- hardware parity beyond documented build/bring-up rituals
- future-facing roadmap areas that are intentionally visible in `docs/roadmaps/`
- parts of the teaching surface that are strong but not yet consolidated into a
  single guided curriculum

## What this repo promises

- clear source access
- documented build paths
- visible caveats
- deterministic proof surfaces where available
- ethics and signing notes that make trust discussable

## What this repo does not promise

- fictional certainty
- hidden maturity claims
- consumer-product smoothness across every path
- that an experimental lane should be read as a finished one

## TODOs

- TODO: add dated, unit-specific bench receipts once repeated calibration runs
  start landing in the docs tree.
- TODO: add recurring dated JUCE smoke entries once actual host/runtime passes
  are being logged with the checklist.

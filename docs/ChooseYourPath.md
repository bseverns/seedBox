# Choose Your Path

You do not need to read the whole repo to begin. Pick the route that matches
what you want from SeedBox right now.

## I want to hear it now

Start with the shortest public overview, then use the native path:

- Read [Why SeedBox](WhySeedBox.md)
- Skim [Current state](CurrentState.md)
- Run the fast-start flow in [README.md](../README.md)
- Try [01 sprout](../examples/01_sprout/README.md)

Best for: first contact, curiosity, quick listening.

## I want to use the native simulator

This is the clearest first technical path in the repo:

- Start in [README.md](../README.md)
- Then read [tests/README.md](../tests/README.md)
- Use [newcomer map](onboarding/newcomer_map.md)
- Explore [examples/03_headless](../examples/03_headless/README.md) after
  `01_sprout`

Best for: learning the system, fast iteration, deterministic experimentation.

## I want to build the hardware

Take the builder path, but read the support boundary first:

- Read [Current state](CurrentState.md)
- Read [Stability and support](StabilityAndSupport.md)
- Then use [builder bootstrap](builder_bootstrap.md)
- Order from [hardware bill of materials](hardware_bill_of_materials.md)
- Bench-check with [calibration guide](calibration_guide.md)

Best for: builders, workshop rigs, physical instrument experiments.

## I want the JUCE / desktop path

Treat JUCE as the desktop body of the same system:

- Read [Current state](CurrentState.md)
- Build from [JUCE builds without the mystery meat](juce_build.md)
- Check [desktop CI builds](ci_desktop_builds.md)
- Use [JUCE manual smoke checklist](JUCESmokeChecklist.md) for runtime checks
- If you want source-level context, open [src/juce/README.md](../src/juce/README.md)

Best for: DAW use, desktop-host experiments, plugin/standalone development.

## I want to understand the architecture

Follow the shared core instead of starting from one platform:

- Read [Why SeedBox](WhySeedBox.md)
- Then [newcomer map](onboarding/newcomer_map.md)
- Then [src/README.md](../src/README.md)
- Keep [include/README.md](../include/README.md) and [tests/README.md](../tests/README.md)
  nearby

Best for: code readers, systems thinkers, DSP collaborators.

## I want to contribute

Start from the current truth and proof surfaces:

- Read [Current state](CurrentState.md)
- Read [Stability and support](StabilityAndSupport.md)
- Review [Seed gallery](SeedGallery.md)
- Use [bench receipt template](BenchReceiptTemplate.md) or [JUCE manual smoke checklist](JUCESmokeChecklist.md) when your contribution needs a runtime receipt
- Then use [builder bootstrap](builder_bootstrap.md), [tests/README.md](../tests/README.md),
  and [scripts/README.md](../scripts/README.md)

Best for: code, docs, tests, hardware, and pedagogy contributions.

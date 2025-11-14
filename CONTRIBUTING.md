# Contributing to SeedBox

Thanks for wanting to tinker with SeedBox. This project is one part synth lab,
one part sketchbook, and we're stoked you want to add your ideas. This guide
explains how we keep the jams coordinated.

## Ground rules (aka the vibe check)

- **Teach as you go.** When you add code, sprinkle comments, diagrams, or docs so
  the next curious hacker can learn from it.
- **Keep the signal clean.** Native and hardware builds should stay in lockstep,
  so gate hardware-only paths behind `#if SEEDBOX_HW`.
- **No memory allocation inside audio callbacks.** Real-time audio is allergic to
  unpredictable latency. Pre-allocate buffers up front or stash work in queues
  outside the callback.

## The PR flow

1. **Talk if it helps.** For big or weird ideas, open a GitHub issue first so we
   can riff together.
2. **Branch with intent.** Name your branches after the experiment: `feat/new-grain-engine`
   or `fix/tempo-drift` beats `patch-1` every time.
3. **Make small, reviewable commits.** Each commit should tell a short story.
4. **Run the checks (see below)** before you push.
5. **Open the PR.** Describe the why, not just the what. Screenshots, audio clips,
   and links to docs are encouraged.
6. **Review is a jam session.** Expect questions, celebrate improvements, and
   keep the tone kind.

## Commit style

- Use present-tense, active voice: `Add grain engine envelope`.
- Reference issues in the body if relevant (`Fixes #42`).
- If your change is visual or documentation-heavy, link to the artifacts you
  created (images, diagrams, etc.).

## Build and test checklist

### Native (desktop) groove check

```bash
pio test -e native
```

This runs the unit/integration suite against the host build. It should be fast,
so run it often while iterating.

### Teensy 4.0 hardware shakedown

```bash
pio run -e teensy40
```

> The retired `teensy40_usbmidiserial` alias is gone from `platformio.ini`; if a
> local script still calls it, swap the env name before the next jam session.

If you're flashing hardware, follow up with a real play test. Listen for xruns,
clicks, or other signs of buffer pain.

### Bonus instrumentation

- `scripts/` holds helper utilities (profiling, version stamping). Document any
  new scripts in `scripts/README.md`.
- When in doubt, drop quick notes into `docs/` so we remember the experiment.

## Docs are first-class

Documentation lives under both MIT (code) and CC BY 4.0 (docs). When you add or
modify docs, cite sources and make them welcoming for the next artist.

## Need help?

Open a discussion or ping the maintainers via the contact info in
[SECURITY.md](SECURITY.md). We're happy to pair-program or brainstorm.

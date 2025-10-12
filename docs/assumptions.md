# SeedBox assumptions & design bets

Welcome to the notebook page where we state our priors loud and clear. These are
the bets we made while shaping SeedBox — the choices that keep the instrument
approachable, hackable, and weird in all the right ways. Treat this like a map
of the knobs we expect to twist (and the ones we promise to leave alone).

## Core musical theses

- **Seeds first, DSP second.** We assume students learn faster when they can poke
  at procedural seeds before diving into wave-shaping math. Everything in
  `src/app/` is tuned around deterministic reseeding so lessons can replay like a
  vinyl loop.
- **Determinism over swagger.** Reseeds, scheduler ticks, even RNG all follow a
  predictable script. We value repeatable experiments more than "surprise"
  randomness because classroom jams need receipts.
- **Quiet by default.** New builds boot with `QUIET_MODE=1`, meaning no seed
  auto-prime, no storage writes, and IO hooks stuck in neutral. You flip the
  switch to make noise, not the other way around.
- **Hardware as an optional flex.** The Teensy rig is a beautiful noise machine,
  but every behavior must run in the native build first. Students can live on a
  laptop and still understand the groove.

## Technical commitments

- **Flag-driven worlds.** Build flags like `SEEDBOX_HW`, `SEEDBOX_SIM`, and now
  `QUIET_MODE` are the contract. If you change behavior, you document which flag
  guards it and why.
- **Storage is disposable until proven otherwise.** Persistent scenes are a
  future feature. Today we assume SD and flash writes are off unless a builder
  explicitly opts in.
- **IO is choreography, not improv.** MIDI routing, display refresh, and storage
  calls are isolated behind interfaces. Quiet mode stubs them so tests can run
  without real hardware thrash.
- **Tests narrate intent.** Every tricky behavior needs a matching test in
  `test/`. If a test fails, we expect the failure message to read like a lab
  note, not a stack trace puzzle.

## Human expectations

- **Docs are part of the show.** Any new subsystem ships with a doc blurb and a
  pointer from this page. If the code moves fast, the notebook moves with it.
- **Students are co-conspirators.** We design APIs so a curious learner can read
  a header and immediately sketch an experiment. Verbose comments beat clever
  one-liners every time.
- **Noise comes with consent.** Booting the real hardware should require an
  explicit "I'm ready to wake the beast" moment. Respect ears, respect venues.

Revisit these assumptions whenever you feel the urge to add a global singleton
or auto-load some mysterious preset. If your new idea breaks a bet, write down
why — future us needs the story as much as the code.

# Test harness — keeping the seeds honest

Tests live here for a reason: determinism is the whole shtick. SeedBox mirrors
the MOARkNOBS-42 vibe by turning every assumption into a check you can run on a
sleep-deprived tour laptop.

## Layout

- `test_app/`
  - Focuses on `AppState` rituals: seeding, reseeding, display snapshots. If the
    UI lies, it's because a test here let it slip.
- `test_patterns/`
  - Exercises the scheduler brain: tick math, seed density, trigger ordering.
    Expand this whenever you touch timing code.

All tests are PlatformIO Unity suites targeting the `native` environment so you
can run them without a Teensy plugged in.

## How to run them

```bash
pio test -e native
```

Run that after every change touching `src/` or `include/`. The native build is
your fast feedback loop, so keep it green.

## Writing new tests

- Narrate your intent in comments. Future contributors should understand the
  scenario without scrolling.
- Seed values should be explicit constants. Deterministic inputs make failures
  reproducible.
- If a bug only shows up on hardware, reproduce the edge case here with a mocked
  dependency, then document the hardware quirk in `docs/`.

When in doubt, over-test. Boring tests keep the live rigs weird in a good way.

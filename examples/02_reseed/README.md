# Example 02 — reseed ritual

See how SeedBox responds when you roll a new master seed. The example prints two
snapshots: the default seed genome and a deterministic reseed with
`0x1234ABCD`.

## Run it

```bash
pio run -e native --project-option "src_dir=examples/02_reseed"
.pio/build/native/program
```

You should see per-seed tone/spread values change while engine assignments stay
stable. Quiet mode keeps IO muted so you can run this during lectures without
surprising the room.

## TODOs

- TODO: Render a 2 s reseed cadence clip to `/out/examples/reseed.wav` once the
  golden harness writes WAVs.
- TODO: Add a CSV diff showing parameter deltas before/after reseed.

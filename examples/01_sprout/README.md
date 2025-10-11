# Example 01 — sprout

This example boots the simulator, runs a few ticks, and prints the initial seed
table. It's the "hello world" for SeedBox's procedural garden.

## Run it

```bash
pio run -e native --project-option "src_dir=examples/01_sprout"
.pio/build/native/program
```

The binary prints the seed list and notes whether quiet mode is active. Because
`QUIET_MODE` defaults to `1`, there is no external IO or randomness beyond the
fixed seed table.

## TODOs

- TODO: Render a 1 s sprout clip to `/out/examples/sprout.wav` and link it here
  once golden fixtures exist.
- TODO: Add a CSV export of the seed table for classroom worksheets.

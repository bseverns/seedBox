# Native golden harness — the mixtape finally pressed to vinyl

The plan from the old zine margin made it to wax. When you flip
`ENABLE_GOLDEN=1` and run the native test environment we now capture both the
raw PCM render **and** the matching hash in a manifest so CI artifacts tell the
truth about what we heard.

## What runs today

1. `tests/native_golden/test_main.cpp` synthesizes a 1-second 110 Hz drone the
   exact same way every time. The fixture lives in
   `build/fixtures/drone-intro.wav` so you can audition it locally without
   spelunking through temp folders.
2. `golden::hash_pcm16` computes the 64-bit FNV-1a fingerprint of that PCM
   payload. We keep the same helper in C++ and Python so the numbers never drift.
3. `scripts/compute_golden_hashes.py` recomputes hashes for every WAV in
   `build/fixtures/` and updates `tests/native_golden/golden.json` with the
   digest, sample rate, frame count, and optional liner notes.
4. The Unity test now refuses to pass if the WAV file is missing **or** the
   manifest forgets about the fixture. Broken receipts? Broken build.

## Refresh loop (aka “cutting a new pressing”)

```bash
# 1. Render the fixtures with golden mode on.
pio test -e native -D ENABLE_GOLDEN=1

# 2. Recompute hashes and commit the manifest update.
python scripts/compute_golden_hashes.py --write

# 3. (Optional) annotate liner notes.
python scripts/compute_golden_hashes.py --note drone-intro="v1 sine reference" --write
```

Need a quick audit without touching disk? Drop the `--write` flag for a dry
run; the script prints a table of fixture names, hashes, and frame counts.

## Manifest anatomy

`tests/native_golden/golden.json` now reads like a record sleeve:

- `generated_at_utc` — timestamp when the manifest was last rebuilt.
- `fixtures[]` — each entry tracks the `name`, `hash`, WAV path, frame count,
  sample rate, channel count, and any notes you scribble for future sleuths.
- `tooling` — declares that both the manifest and hash math come from
  `scripts/compute_golden_hashes.py` so newcomers know which lever to pull.

That manifest doubles as documentation in the new
[`docs/roadmaps/native_golden.md`](../../docs/roadmaps/native_golden.md) entry,
so remember to capture any future twists there as you add fixtures or widen the
rendering matrix.

## Future adventures

- Drop more engines into the harness and layer multi-track renders so we can
  regression-test mix balances, not just sine drones.
- Wire CI to stash the emitted WAVs under `artifacts/` whenever `ENABLE_GOLDEN`
  is enabled so we get instant playback links on PRs.
- Extend the manifest schema with semantic tags (`engine="granular"`) so the
  docs can map fixtures back to their roadmaps/tests automatically.

# Native golden harness — the mixtape finally pressed to vinyl

The plan from the old zine margin made it to wax. When you flip
`ENABLE_GOLDEN=1` and run the native test environment we now capture both the
raw PCM render **and** the matching hash in a manifest so CI artifacts tell the
truth about what we heard.

## What runs today

1. `tests/native_golden/test_main.cpp` still prints the 1-second 110 Hz drone to
   `build/fixtures/drone-intro.wav`, but it also forges a sampler chord stack,
   a resonator tail collage, and deterministic Euclid/Burst debug logs. The new
   WAVs (`sampler-grains.wav`, `resonator-tail.wav`) and logs
   (`euclid-mask.txt`, `burst-cluster.txt`) live beside the original drone so
   reviewers can audition or diff each engine in isolation. PlatformIO now
   injects the absolute repo path as `SEEDBOX_PROJECT_ROOT_HINT`, the harness
   honors a `SEEDBOX_PROJECT_ROOT` override when you need to aim somewhere
   bespoke, and it still walks up to the nearest `platformio.ini` for safety.
   The fixtures land in `<repo>/build/fixtures` even though PlatformIO executes
   the binary from `.pio/build/*/test`.
2. `golden::hash_pcm16` handles the PCM renders while a tiny FNV-1a byte helper
   fingerprints the log files. Both mirror `scripts/compute_golden_hashes.py`
   so the manifest hashes match what the test harness expects.
3. `scripts/compute_golden_hashes.py` now scans `build/fixtures/` for both WAV
   and `.txt` artifacts and updates `tests/native_golden/golden.json` with their
   hashes, audio metadata (rate/frames/channels), or log metadata (line + byte
   counts). Drop `--note name="liner note"` to annotate any entry.
4. The Unity test refuses to pass if **any** declared artifact is missing or if
   the manifest hash falls out of sync. Broken receipts? Broken build.

## Refresh loop (aka “cutting a new pressing”)

```bash
# 1. Render the fixtures with golden mode on.
pio test -e native_golden

# 2. Recompute hashes and commit the manifest update.
python scripts/compute_golden_hashes.py --write

# 3. (Optional) annotate liner notes.
python scripts/compute_golden_hashes.py --note drone-intro="v1 sine reference" --write
```

That dedicated env mirrors the vanilla native build but bakes in the flag so we
don't accidentally reuse a stale binary and skip fixture writes in CI.

Need a quick audit without touching disk? Drop the `--write` flag for a dry
run; the script prints a table of fixture names, hashes, and frame counts. If
you're experimenting in a scratch directory, point the harness somewhere else
with `SEEDBOX_FIXTURE_ROOT=/tmp/seedbox-fixtures pio test -e native_golden`.
Pair it with `SEEDBOX_PROJECT_ROOT=/path/to/seedBox` if you launched from a
quirky working directory and want to skip the auto-discovery. The manifest
still records the canonical `build/fixtures/...` paths, but the files
themselves can live wherever makes debugging painless.

## Manifest anatomy

`tests/native_golden/golden.json` now reads like a record sleeve:

- `generated_at_utc` — timestamp when the manifest was last rebuilt.
- `fixtures[]` — every entry exposes the `name`, `kind` (`audio` or `log`),
  the filesystem `path`, FNV hash, and either audio metadata (sample rate,
  frame + channel counts) or log metadata (byte + line counts). Notes are still
  fair game for scribbling context.
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
